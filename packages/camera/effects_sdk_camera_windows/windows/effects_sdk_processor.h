#ifndef PACKAGES_CAMERA_CAMERA_WINDOWS_WINDOWS_EFFECTS_SDK_PROCESSOR_HANDLER_H_
#define PACKAGES_CAMERA_CAMERA_WINDOWS_WINDOWS_EFFECTS_SDK_PROCESSOR_HANDLER_H_

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <Windows.h>

#include "include_effects_sdk/frame.h"
#include "include_effects_sdk/pipeline.h"
#include "include_effects_sdk/frame_factory.h"
#include "include_effects_sdk/sdk_factory.h"

namespace camera_windows {

class Releaser
{
public:
	void operator()(::tsvb::IRelease* object)
	{
		object->release();
	}
};

class EffectsSDKProcessor {
 public:
  EffectsSDKProcessor();
  ~EffectsSDKProcessor();

  void LibraryInit(const std::string& library_url);

  void SetBlur(float blurPower);
  void ClearBlur();
  // void SetSegmentationPreset(std::string& preset);
  void SetBeautificationLevel(float level);
  void ClearBeautification();
  void SetBackgroundImage(const std::string& url);
  void SetBackgroundColor(int color);
  void ClearBackground();

  uint8_t* Process(uint8_t* camera_frame);

  void UpdateResolution(uint32_t width, uint32_t height);

  bool AnyEffectActive() const { return blurOn_ || beautificationOn_ || backgroundOn_; }

 private:
  bool blurOn_ = false;
  bool beautificationOn_ = false;
  bool backgroundOn_ = false;

  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint32_t stride_ = 0;

  float blur_power_ = -1.f;
  float beautification_level_ = -1.f;

  std::unique_ptr<::tsvb::IFrameFactory, Releaser> frame_factory_;
  std::unique_ptr<::tsvb::IReplacementController, Releaser> replacement_controller_;
  std::unique_ptr<::tsvb::IPipeline, Releaser> pipeline_;

  std::unique_ptr<::tsvb::IFrame, Releaser> background_image_;

  ::tsvb::IFrame* initial_frame_;
  ::tsvb::ILockedFrameData* initial_frame_new_;

  ::tsvb::IFrame* processed_frame_;
  ::tsvb::ILockedFrameData* processed_frame_data_;

  ::tsvb::pfnCreateSDKFactory create_SDK_Factory_ = nullptr;
  HMODULE dll_handle_ = nullptr;

  std::vector<uint8_t> frame_data_;
};

}  // namespace camera_windows

#endif  // PACKAGES_CAMERA_CAMERA_WINDOWS_WINDOWS_EFFECTS_SDK_PROCESSOR_HANDLER_H_
