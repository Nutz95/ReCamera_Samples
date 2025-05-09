#ifndef _MA_MODEL_YOLOV8_H
#define _MA_MODEL_YOLOV8_H

#include <vector>

#include "ma_model_detector.h"

namespace ma::model {

class YoloV8 : public Detector {
private:
    ma_tensor_t outputs_[6];
    int32_t num_record_;
    int32_t num_class_;

protected:
    ma_err_t postprocess() override;
    ma_err_t postProcessI8();

    ma_err_t postProcessF32();


public:
    YoloV8(Engine* engine);
    ~YoloV8();
    static bool isValid(Engine* engine);
};

}  // namespace ma::model

#endif  // _MA_MODEL_YOLO_H
