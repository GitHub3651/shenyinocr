#ifndef BARCODEDECODER_H
#define BARCODEDECODER_H

#include "BarcodeDecoderApi.h"
#include "BarcodeTypes.h"

#include <QLibrary>
#include <QString>

class BarcodeDecoder
{
public:
    explicit BarcodeDecoder(const QString &libraryFilePath = QString());
    ~BarcodeDecoder();

    bool load(const QString &libraryFilePath = QString());
    void unload();

    bool isAvailable() const;
    QString version() const;
    QString libraryFilePath() const;
    QString lastError() const;

    BarcodeReadResult decode(
        const cv::Mat &image,
        const BarcodeDecodeOptions &options = BarcodeDecodeOptions());

private:
    BarcodeReadResult decodeOnce(
        const cv::Mat &grayImage,
        unsigned int formatMask,
        unsigned int optionFlags) const;

    static bool isFatalDecodeStatus(BarcodeReadStatus status);
    static BarcodeFormat toBarcodeFormat(int decoderFormat);

    QLibrary m_library;
    BarcodeDecoderGetVersionFunction m_getVersion;
    BarcodeDecoderDecodeLuma8Function m_decodeLuma8;
    QString m_libraryFilePath;
    QString m_version;
    QString m_lastError;
};

#endif // BARCODEDECODER_H
