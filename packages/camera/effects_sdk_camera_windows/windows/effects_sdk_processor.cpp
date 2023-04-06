#include "effects_sdk_processor.h"

#include <cassert>

#include <algorithm>

#define DEFAULT_VB_NAME "tsvb.dll"

namespace camera_windows {

void Releaser::operator() (::tsvb::IRelease* object) {
  object->release();
}

EffectsSDKProcessor::EffectsSDKProcessor() {
  LibraryInit(DEFAULT_VB_NAME);
}

EffectsSDKProcessor::~EffectsSDKProcessor() {
  if (frame_factory_)
    frame_factory_.reset();

  if (replacement_controller_)
    replacement_controller_.reset();

  if (pipeline_)
    pipeline_.reset();

  if (background_image_)
    background_image_.reset();

  if (initial_frame_)
    initial_frame_.reset();

  if (initial_frame_data_)
    initial_frame_data_.reset();

  if (processed_frame_)
    processed_frame_.reset();

  if (processed_frame_data_)
    processed_frame_data_.reset();

  create_SDK_Factory_ = nullptr;
  if (nullptr != dll_handle_) {
    FreeLibrary(dll_handle_);
    dll_handle_ = nullptr;
  }
}

void EffectsSDKProcessor::LibraryInit(const std::string& library_url) {
    std::string directory = "";
    size_t last_slash_idx = library_url.rfind('\\');
    if (std::string::npos != last_slash_idx)
      directory = library_url.substr(0, last_slash_idx);
    else
      last_slash_idx = library_url.rfind('/');

    if (std::string::npos != last_slash_idx)
      directory = library_url.substr(0, last_slash_idx);

    if (directory != "") { 
      std::wstring w_directory = std::wstring(directory.begin(), directory.end());
      PCWSTR w_c_directory = w_directory.c_str();
      ::AddDllDirectory(w_c_directory);

      ::SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_USER_DIRS       |
                                 LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
                                 LOAD_LIBRARY_SEARCH_DEFAULT_DIRS    | 
                                 LOAD_LIBRARY_SEARCH_SYSTEM32        );
    }

  std::wstring w_url = std::wstring(library_url.begin(), library_url.end());
  LPCWSTR w_c_url = w_url.c_str();

  dll_handle_ = ::LoadLibrary(w_c_url);
  if (nullptr == dll_handle_) {
    return;
  }

  create_SDK_Factory_ = reinterpret_cast<::tsvb::pfnCreateSDKFactory>(
    GetProcAddress(dll_handle_, "createSDKFactory")
  );

  auto sdk_factory = create_SDK_Factory_();

  frame_factory_.reset(sdk_factory->createFrameFactory());
  pipeline_.reset(sdk_factory->createPipeline());
  
  sdk_factory->release();
}

void EffectsSDKProcessor::SetBlur(float blurPower) {
  if (pipeline_ && !blurOn_) {
    blur_power_ = blurPower;
    pipeline_->enableBlurBackground(blur_power_);
    blurOn_ = true;
  }
}

void EffectsSDKProcessor::ClearBlur() {
  if (pipeline_ && blurOn_) {
    blurOn_ = false;
    pipeline_->disableBackgroundBlur();
    blur_power_ = -1.f;
  }  
}

// void EffectsSDKProcessor::SetSegmentationPreset(std::string& preset) {
// }

void EffectsSDKProcessor::SetBeautificationLevel(float level) {
  if (pipeline_ && !beautificationOn_) {
    beautification_level_ = level;
    pipeline_->setBeautificationLevel(beautification_level_);
    pipeline_->enableBeautification();
    beautificationOn_ = true;
  }
}

void EffectsSDKProcessor::ClearBeautification() {
  if (pipeline_ && beautificationOn_) {
    beautification_level_ = -1.f;
    pipeline_->disableBeautification();
    beautificationOn_ = false;
  }
}

void EffectsSDKProcessor::SetBackgroundImage(const std::string& url) {
  if (pipeline_ && !backgroundOn_) {
    ::tsvb::IReplacementController* controller = nullptr;
    auto error = pipeline_->enableReplaceBackground(&controller);

    // TODO: Refacore error
    if (error != ::tsvb::PipelineErrorCode::ok)
      throw std::runtime_error("Can't enable replace background");

    replacement_controller_.reset(controller);

    background_image_.reset(frame_factory_->loadImage(url.c_str()));

    replacement_controller_->setBackgroundImage(background_image_.get());

    backgroundOn_ = true;
  }
}

void EffectsSDKProcessor::SetBackgroundColor(int color) {
  if (pipeline_ && !backgroundOn_) {
    ::tsvb::IReplacementController* controller = nullptr;
    auto error = pipeline_->enableReplaceBackground(&controller);

    // TODO: Refacore error
    if (error != ::tsvb::PipelineErrorCode::ok)
      throw std::runtime_error("Can't enable replace background");

    replacement_controller_.reset(controller);

    const int size = width_ * height_ * 4;
    std::vector<int8_t> bgra_data(size, 0);
    bgra_data.insert(bgra_data.begin(), size, 0);

    const int8_t b = (color >> 24) & 0xFF;
    const int8_t g = (color >> 16) & 0xFF;
    const int8_t r = (color >> 8) & 0xFF;
    const int8_t a = (color >> 0) & 0xFF;
    
    for (int i = 0; i < size; i += 4) {
      bgra_data[i] = b;
      bgra_data[i + 1] = g;
      bgra_data[i + 2] = r;
      bgra_data[i + 3] = a;
    }

    auto background = frame_factory_->createBGRA(bgra_data.data(), width_ * 4, width_, height_, true);
    replacement_controller_->setBackgroundImage(background);
    background->release();

    backgroundOn_ = true;
  }
}

void EffectsSDKProcessor::ClearBackground() {
  if (replacement_controller_ && backgroundOn_) {
    backgroundOn_ = false;
    replacement_controller_->clearBackgroundImage();
  }
}

uint8_t* EffectsSDKProcessor::Process(uint8_t* camera_frame) {
  // if (!blurOn_ && !beautificationOn_ && !backgroundOn_)
  //   return camera_frame;

  initial_frame_.reset(frame_factory_->createBGRA(camera_frame, width_ * 4, width_, height_, false));

  processed_frame_.reset(pipeline_->process(initial_frame_.get(), &pipeline_error_));

  initial_frame_data_.reset(initial_frame_->lock(::tsvb::FrameLock::readWrite));

  // TODO: Refacore error
  if (pipeline_error_ != ::tsvb::PipelineErrorCode::ok)
    throw std::runtime_error("Can't process frame");

  processed_frame_data_.reset(processed_frame_->lock(::tsvb::FrameLock::readWrite));
  stride_ = initial_frame_data_->bytesPerLine(0);

  auto rgba_data = reinterpret_cast<uint8_t*>(processed_frame_data_->dataPointer(0));
  uint8_t tmp_r = 0;
  for (uint32_t i = 0; i < height_; i++) {
    for (uint32_t j = 0; j < width_ * 4; j += 4) { 
      tmp_r = rgba_data[j];
      rgba_data[j] = rgba_data[j + 2];
      rgba_data[j + 2] = tmp_r;
    }
    rgba_data += stride_;
  }

  memcpy(frame_data_.data(), rgba_data - stride_ * height_, stride_ * height_);
  
  return frame_data_.data();
}

void EffectsSDKProcessor::UpdateResolution(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;
  frame_data_.resize(width_ * height_ * 4);
}

}