#pragma once

#include "ROSBaseMsg.h"
#include "std_msgs/Header.h"

namespace ROSMessages {
namespace sensor_msgs {
class CompressedImage : public FROSBaseMsg {
 public:
  CompressedImage() {
    _MessageType = "sensor_msgs/CompressedImage";
  }

  /** Header timestamp should be acquisition time of image
   *  Header frame_id should be optical frame of camera
   *  origin of frame should be optical center of camera
   *  +x should point to the right in the image
   *  +y should point down in the image
   *  +z should point into to plane of the image
   *  If the frame_id here and the frame_id of the CameraInfo
   *  message associated with the image conflict
   *  the behavior is undefined
   */
  ROSMessages::std_msgs::Header header;

  // The legal values for encoding are in file src / image_encodings.cpp
  // If you want to standardize a new string format, join
  // ros - users@lists.sourceforge.net and send an email proposing a new encoding.
  FString format;		// Encoding of pixels -- channel meaning, ordering, size taken from the list of strings in include / sensor_msgs / image_encodings.h

  // To avoid copy operations of the image data,
  // hand over a pointer to the uint8 data.
  // Please note, that the memory this pointer points to must be valid until this message has been published.
  const uint8* data;		// actual matrix data, size is (step * rows)
};
}
}