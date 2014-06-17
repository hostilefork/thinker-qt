/****************************************************************************
**
** Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Modified 2010 by HostileFork to use http://hostilefork.com/thinker-qt/
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef RENDERTHREAD_H
#define RENDERTHREAD_H

#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include <QImage>
#include "thinkerqt/thinker.h"

class RenderThinker;

class RenderThinkerData : public SnapshottableData
{
private:
    friend class RenderThinker;
    QImage image;
    double scaleFactor;

public:
    RenderThinkerData ()
    { }

    RenderThinkerData (const RenderThinkerData& other)
        : image (other.image),
        scaleFactor(other.scaleFactor)
    { }

    bool hasImage() const { return !image.isNull(); }

    QImage const & getImage() const { return image; }

    double getScaleFactor() const { return scaleFactor; }
};

const int ColormapSize = 512;
typedef uint Colormap[ColormapSize];

class RenderThinker : public Thinker<RenderThinkerData>
{
    Q_OBJECT

public:
    RenderThinker(double centerX, double centerY, double scaleFactor,
                QSize resultSize, const Colormap& colormap);

protected:
    virtual void start() override;

private:
    double centerX;
    double centerY;
    double scaleFactor;
    QSize resultSize;
    const Colormap& colormap;
};

#endif
