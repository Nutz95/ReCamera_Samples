#include "barcode_decoder_zx.h"
#include <ZXing/ReadBarcode.h>
#include <ZXing/ZXVersion.h>
#include <ZXing/ZXingCpp.h>
#include <ZXing/Result.h>
#include <ZXing/BarcodeFormat.h>
#include <ZXing/DecodeHints.h>
#include <ZXing/ImageView.h>
#include <opencv2/imgproc.hpp>

BarcodeDecoderZX::Result BarcodeDecoderZX::decode(const cv::Mat& input) {
    // Convertir en niveaux de gris si besoin
    cv::Mat gray;
    if (input.channels() == 1) {
        gray = input;
    } else {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    }

    ZXing::ImageView imageView(gray.data, gray.cols, gray.rows, ZXing::ImageFormat::Lum, gray.step);
    ZXing::DecodeHints hints;
    hints.setTryHarder(true);
    auto result = ZXing::ReadBarcode(imageView, hints);

    BarcodeDecoderZX::Result out;
    out.success = result.isValid();
    out.text = result.text();
    out.format = ZXing::ToString(result.format());
    return out;
}
