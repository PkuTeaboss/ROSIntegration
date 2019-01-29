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

  ROSMessages::std_msgs::Header header;

  FString format;

  const uint8* data;

  int data_size;

};
}
}