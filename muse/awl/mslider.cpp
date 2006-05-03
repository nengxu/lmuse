//=============================================================================
//  Awl
//  Audio Widget Library
//  $Id:$
//
//  Copyright (C) 2002-2006 by Werner Schweer and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================

#include "fastlog.h"

#include "mslider.h"

namespace Awl {

//---------------------------------------------------------
//   MeterSlider
//---------------------------------------------------------

MeterSlider::MeterSlider(QWidget* parent)
   : VolSlider(parent)
      {
      _channel    = 0;
      yellowScale = -10;
      redScale    = 0;
      _meterWidth = _scaleWidth * 3;
      setChannel(1);
      }

//---------------------------------------------------------
//   sizeHint
//---------------------------------------------------------

QSize MeterSlider::sizeHint() const
  	{
      int w = _meterWidth + _scaleWidth + _scaleWidth + 30;
 	return orientation() == Qt::Vertical ? QSize(w, 200) : QSize(200, w);
      }

//---------------------------------------------------------
//   setChannel
//---------------------------------------------------------

void MeterSlider::setChannel(int n)
      {
      if (n > _channel) {
            for (int i = _channel; i < n; ++i) {
                  meterval.push_back(0.0f);
                  meterPeak.push_back(0.0f);
                  }
            }
      _channel = n;
      }

//---------------------------------------------------------
//   setMeterVal
//---------------------------------------------------------

void MeterSlider::setMeterVal(int channel, float v, float peak)
      {
      bool mustRedraw = false;
      if (meterval[channel] != v) {
            meterval[channel] = v;
            mustRedraw = true;
            }
      if (peak != meterPeak[channel]) {
            meterPeak[channel] = peak;
            mustRedraw = true;
            }
      if (mustRedraw)
            update();
      }

//---------------------------------------------------------
//   resetPeaks
//    reset peak and overflow indicator
//---------------------------------------------------------

void MeterSlider::resetPeaks()
      {
      for (int i = 0; i < _channel; ++i)
            meterPeak[i]  = meterval[i];
      update();
      }

//---------------------------------------------------------
//   paint
//---------------------------------------------------------

void MeterSlider::paint(const QRect& r)
      {
      int pixel = height() - sliderSize().height();
      double range = maxValue() - minValue();
      int ppos = int(pixel * (_value - minValue()) / range);
      if (_invert)
            ppos = pixel - ppos;

      QRect rr(r);
      QPainter p(this);

      QColor sc(isEnabled() ? _scaleColor : Qt::gray);
      QColor svc(isEnabled() ? _scaleValueColor : Qt::gray);
      p.setBrush(svc);

      int h  = height();
      int kh = sliderSize().height();

      //---------------------------------------------------
      //    draw meter
      //---------------------------------------------------

      int mw = _meterWidth / _channel;
      int x  = 20;

      int y1 = kh / 2;
      int y2 = h - (ppos + y1);
      int y3 = h - y1;

      int mh  = h - kh;
      int h1 = mh - lrint((maxValue() - redScale) * mh / range);
      int h2 = mh - lrint((maxValue() - yellowScale) * mh / range);

      p.setPen(QPen(Qt::white, 2));
      for (int i = 0; i < _channel; ++i) {
            int h = mh - (lrint(fast_log10(meterval[i]) * -20.0f * mh / range));
            if (h < 0)
                  h = 0;
            else if (h > mh)
                  h = mh;
            if (h > h1) {
                  p.fillRect(x, y3-h2, mw, h2,      QBrush(0x00ff00));  // green
                  p.fillRect(x, y3-h1, mw, h1 - h2, QBrush(0xffff00));  // yellow
                  p.fillRect(x, y3-h,  mw, h - h1,  QBrush(0xff0000));  // red
                  p.fillRect(x, y1,    mw, mh - h,  QBrush(0x8e0000));  // dark red
                  }
            else if (h > h2) {
                  p.fillRect(x, y3-h2, mw, h2, QBrush(0x00ff00));       // green
                  p.fillRect(x, y3-h,  mw,  h-h2, QBrush(0xffff00));    // yellow
                  p.fillRect(x, y3-h1, mw,  h1-h, QBrush(0x8e8e00));    // dark yellow
                  p.fillRect(x, y1,    mw, mh - h1,  QBrush(0x8e0000)); // dark red
                  }
            else {
                  p.fillRect(x, y3-h, mw, h,         QBrush(0x00ff00)); // green
                  p.fillRect(x, y3-h2, mw, h2-h,     QBrush(0x007000)); // dark green
                  p.fillRect(x, y3-h1,   mw,  h1-h2, QBrush(0x8e8e00)); // dark yellow
                  p.fillRect(x, y1,   mw,  mh - h1,  QBrush(0x8e0000)); // dark red
                  }

            //---------------------------------------------------
            //    draw peak line
            //---------------------------------------------------

            h = mh - (lrint(fast_log10(meterPeak[i]) * -20.0f * mh / range));
            p.drawLine(x, y3-h, x+mw, y3-h);

            x += mw;
            }

      //---------------------------------------------------
      //    draw scale
      //---------------------------------------------------

      p.fillRect(x, y1, _scaleWidth, y2-y1, sc);
      p.fillRect(x, y2, _scaleWidth, y3-y2, svc);

      //---------------------------------------------------
      //    draw tick marks
      //---------------------------------------------------

  	QFont f(p.font());
   	f.setPointSize(6);
   	p.setFont(f);
      p.setPen(QPen(Qt::darkGray, 2));
   	QFontMetrics fm(f);
      int xt = 20 - fm.width("00") - 5;

      QString s;
   	for (int i = 10; i < 70; i += 10) {
      	h  = y1 + lrint(i * mh / range);
         	s.setNum(i - 10);
  		p.drawText(xt,  h - 3, s);
		p.drawLine(15, h, 20, h);
         	}

      //---------------------------------------------------
      //    draw slider
      //---------------------------------------------------

      x  += _scaleWidth/2;
      p.setPen(QPen(svc, 0));
      p.translate(QPointF(x, y2));
      p.setRenderHint(QPainter::Antialiasing, true);
      p.drawPath(*points);
      }

//---------------------------------------------------------
//   mousePressEvent
//---------------------------------------------------------

void MeterSlider::mousePressEvent(QMouseEvent* ev)
      {
      if (ev->pos().x() < _meterWidth) {
            emit meterClicked();
            return;
            }
      VolSlider::mousePressEvent(ev);
      }
}

