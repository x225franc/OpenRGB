/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QImage>
#include <algorithm>

// Two running-sum box passes: bounded work per frame, independent of LED count.
// This immutable image is shared by physical zones and the preview.
inline QImage AmbientSpatialNeighborhood(const QImage& input)
{
    if(input.isNull()) return {};
    QImage source = input;
    if(std::max(source.width(), source.height()) > 640)
        source = source.scaled(640, 640, Qt::KeepAspectRatio, Qt::FastTransformation);
    QImage horizontal(source.size(), QImage::Format_RGB32);
    QImage result(source.size(), QImage::Format_RGB32);
    const int w = source.width(), h = source.height();
    auto pass = [&](const QImage& src, QImage& dst, bool vertical)
    {
        const int length = vertical ? h : w, lines = vertical ? w : h;
        const int radius = std::max(1, length / 20);
        for(int line = 0; line < lines; ++line)
        {
            int sums[3] = {0,0,0}, low = 0, high = -1;
            auto add = [&](int index, int sign)
            {
                const auto* row = reinterpret_cast<const QRgb*>(src.constScanLine(vertical ? index : line));
                const QRgb pixel = row[vertical ? line : index];
                sums[0] += sign*qRed(pixel); sums[1] += sign*qGreen(pixel); sums[2] += sign*qBlue(pixel);
            };
            for(int i = 0; i < length; ++i)
            {
                while(high < std::min(length-1, i+radius)) add(++high, 1);
                while(low < std::max(0, i-radius)) add(low++, -1);
                const int count = high-low+1;
                auto* row = reinterpret_cast<QRgb*>(dst.scanLine(vertical ? i : line));
                row[vertical ? line : i] = qRgb((sums[0]+count/2)/count, (sums[1]+count/2)/count, (sums[2]+count/2)/count);
            }
        }
    };
    pass(source, horizontal, false);
    pass(horizontal, result, true);
    return result;
}
