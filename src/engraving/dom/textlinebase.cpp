/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "textlinebase.h"

#include <cmath>

#include "draw/types/pen.h"

#include "factory.h"
#include "score.h"
#include "system.h"
#include "text.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   TextLineBaseSegment
//---------------------------------------------------------

TextLineBaseSegment::TextLineBaseSegment(const ElementType& type, Spanner* sp, System* parent, ElementFlags f)
    : LineSegment(type, sp, parent, f)
{
    m_text    = Factory::createText(this, TextStyleType::DEFAULT, false);
    m_endText = Factory::createText(this, TextStyleType::DEFAULT, false);
    m_text->setParent(this);
    m_endText->setParent(this);
    m_text->setFlag(ElementFlag::MOVABLE, false);
    m_endText->setFlag(ElementFlag::MOVABLE, false);
}

TextLineBaseSegment::TextLineBaseSegment(const TextLineBaseSegment& seg)
    : LineSegment(seg)
{
    m_text    = seg.m_text->clone();
    m_endText = seg.m_endText->clone();
    m_text->setParent(this);
    m_endText->setParent(this);
    // set the right _text
    renderer()->layoutTextLineBaseSegment(this);
}

TextLineBaseSegment::~TextLineBaseSegment()
{
    delete m_text;
    delete m_endText;
}

//---------------------------------------------------------
//   setSelected
//---------------------------------------------------------

void TextLineBaseSegment::setSelected(bool f)
{
    SpannerSegment::setSelected(f);
    m_text->setSelected(f);
    m_endText->setSelected(f);
}

RectF TextLineBaseSegment::boundingBoxOfLine(const PointF& p1, const PointF& p2, double lw2, bool isDottedLine)
{
    if (isDottedLine) {
        return RectF(p1, p2).normalized().adjusted(-lw2, -lw2, lw2, lw2);
    }

    PointF a = lw2 * (p2 - p1).normalized();
    PointF b(-a.y(), a.x());
    return RectF(p1 - b, p1 + b).normalized().united(RectF(p2 - b, p2 + b).normalized());
}

bool TextLineBaseSegment::setProperty(Pid id, const PropertyValue& v)
{
    if (id == Pid::COLOR) {
        Color color = v.value<Color>();

        if (m_text) {
            m_text->setColor(color);
        }

        if (m_endText) {
            m_endText->setColor(color);
        }
    }

    return LineSegment::setProperty(id, v);
}

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void TextLineBaseSegment::spatiumChanged(double ov, double nv)
{
    LineSegment::spatiumChanged(ov, nv);

    textLineBase()->spatiumChanged(ov, nv);
    m_text->spatiumChanged(ov, nv);
    m_endText->spatiumChanged(ov, nv);
}

static constexpr std::array<Pid, 31> TextLineBasePropertyId = { {
    Pid::LINE_VISIBLE,
    Pid::BEGIN_HOOK_TYPE,
    Pid::BEGIN_HOOK_HEIGHT,
    Pid::END_HOOK_TYPE,
    Pid::END_HOOK_HEIGHT,
    Pid::GAP_BETWEEN_TEXT_AND_LINE,
    Pid::BEGIN_TEXT,
    Pid::BEGIN_TEXT_ALIGN,
    Pid::BEGIN_TEXT_PLACE,
    Pid::BEGIN_FONT_FACE,
    Pid::BEGIN_FONT_SIZE,
    Pid::BEGIN_FONT_STYLE,
    Pid::BEGIN_TEXT_OFFSET,
    Pid::CONTINUE_TEXT,
    Pid::CONTINUE_TEXT_ALIGN,
    Pid::CONTINUE_TEXT_PLACE,
    Pid::CONTINUE_FONT_FACE,
    Pid::CONTINUE_FONT_SIZE,
    Pid::CONTINUE_FONT_STYLE,
    Pid::CONTINUE_TEXT_OFFSET,
    Pid::END_TEXT,
    Pid::END_TEXT_ALIGN,
    Pid::END_TEXT_PLACE,
    Pid::END_FONT_FACE,
    Pid::END_FONT_SIZE,
    Pid::END_FONT_STYLE,
    Pid::END_TEXT_OFFSET,
    // Text (global)
    //Pid::TEXT_STYLE,
    Pid::FONT_FACE,
    Pid::FONT_SIZE,
    Pid::FONT_STYLE,
    /*Pid::TEXT_LINE_SPACING,
    /*Pid::FRAME_TYPE,
    Pid::FRAME_WIDTH,
    Pid::FRAME_PADDING,
    Pid::FRAME_ROUND,
    Pid::FRAME_FG_COLOR,
    Pid::FRAME_BG_COLOR,*/
    //Pid::TEXT,
    Pid::ALIGN,
    //Pid::TEXT_SCRIPT_ALIGN,
    //Pid::TEXT_LINKED_TO_MASTER,
} };

const std::array<Pid, 31>& TextLineBase::textLineBasePropertyIds()
{
    return TextLineBasePropertyId;
}

void TextLineBase::reset()
{
    String beginText = _beginText;
    String contText = _continueText;
    String endText = _endText;
    bool beginStyled = isStyled(Pid::BEGIN_TEXT);
    bool contStyled = isStyled(Pid::CONTINUE_TEXT);
    bool endStyled = isStyled(Pid::END_TEXT);

    SLine::reset();
    if (!beginStyled) {
        undoChangeProperty(Pid::BEGIN_TEXT, beginText, PropertyFlags::UNSTYLED);
    }
    if (!contStyled) {
        undoChangeProperty(Pid::CONTINUE_TEXT, contText, PropertyFlags::UNSTYLED);
    }
    if (!endStyled) {
        undoChangeProperty(Pid::END_TEXT, endText, PropertyFlags::UNSTYLED);
    }
}

//---------------------------------------------------------
//   propertyDelegate
//---------------------------------------------------------

EngravingItem* TextLineBaseSegment::propertyDelegate(Pid pid)
{
    for (Pid id : TextLineBasePropertyId) {
        if (pid == id) {
            return spanner();
        }
    }
    return LineSegment::propertyDelegate(pid);
}

//---------------------------------------------------------
//   TextLineBase
//---------------------------------------------------------

TextLineBase::TextLineBase(const ElementType& type, EngravingItem* parent, ElementFlags f)
    : SLine(type, parent, f)
{
    setBeginHookHeight(Spatium(1.9));
    setEndHookHeight(Spatium(1.9));
    setGapBetweenTextAndLine(Spatium(0.5));
}

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void TextLineBase::spatiumChanged(double /*ov*/, double /*nv*/)
{
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue TextLineBase::getProperty(Pid id) const
{
    switch (id) {
    // Text (global)
    //case Pid::TEXT_STYLE:
    case Pid::FONT_FACE: {
		if (_beginFontFamily == _continueFontFamily && _beginFontFamily == _endFontFamily) {
			return _beginFontFamily;
		} else {
			return PropertyValue();
		}
	}
    case Pid::FONT_SIZE: {
		if (_beginFontSize == _continueFontSize && _beginFontSize == _endFontSize) {
			return _beginFontSize;
		} else {
			return PropertyValue();
		}
	}
    case Pid::FONT_STYLE: {
		if (int(_beginFontStyle) == int(_continueFontStyle) && int(_beginFontStyle) == int(_endFontStyle)) {
			return int(_beginFontStyle);
		} else {
			return PropertyValue();
		}
	}
    //case Pid::TEXT_LINE_SPACING:
    /*case Pid::FRAME_TYPE:
    case Pid::FRAME_WIDTH:
    case Pid::FRAME_PADDING:
    case Pid::FRAME_ROUND:
    case Pid::FRAME_FG_COLOR:
    case Pid::FRAME_BG_COLOR:*/
    case Pid::ALIGN: {
		if (beginTextAlign() == continueTextAlign() && beginTextAlign() == endTextAlign()) {
			return PropertyValue::fromValue(beginTextAlign());
		} else {
			return PropertyValue();
		}
	}
    //case Pid::TEXT_SCRIPT_ALIGN:
    case Pid::TEXT: {
		if (beginText() == continueText() && beginText() == endText()) {
			return beginText();
		} else {
			return PropertyValue();
		}
	}
    //case Pid::TEXT_LINKED_TO_MASTER:
    case Pid::BEGIN_TEXT:
        return beginText();
    case Pid::BEGIN_TEXT_ALIGN:
        return PropertyValue::fromValue(beginTextAlign());
    case Pid::CONTINUE_TEXT_ALIGN:
        return PropertyValue::fromValue(continueTextAlign());
    case Pid::END_TEXT_ALIGN:
        return PropertyValue::fromValue(endTextAlign());
    case Pid::BEGIN_TEXT_PLACE:
        return _beginTextPlace;
    case Pid::BEGIN_HOOK_TYPE:
        return _beginHookType;
    case Pid::BEGIN_HOOK_HEIGHT:
        return _beginHookHeight;
    case Pid::BEGIN_FONT_FACE:
        return _beginFontFamily;
    case Pid::BEGIN_FONT_SIZE:
        return _beginFontSize;
    case Pid::BEGIN_FONT_STYLE:
        return int(_beginFontStyle);
    case Pid::BEGIN_TEXT_OFFSET:
        return _beginTextOffset;
    case Pid::CONTINUE_TEXT:
        return continueText();
    case Pid::CONTINUE_TEXT_PLACE:
        return _continueTextPlace;
    case Pid::CONTINUE_FONT_FACE:
        return _continueFontFamily;
    case Pid::CONTINUE_FONT_SIZE:
        return _continueFontSize;
    case Pid::CONTINUE_FONT_STYLE:
        return int(_continueFontStyle);
    case Pid::CONTINUE_TEXT_OFFSET:
        return _continueTextOffset;
    case Pid::END_TEXT:
        return endText();
    case Pid::END_TEXT_PLACE:
        return _endTextPlace;
    case Pid::END_HOOK_TYPE:
        return _endHookType;
    case Pid::END_HOOK_HEIGHT:
        return _endHookHeight;
    case Pid::GAP_BETWEEN_TEXT_AND_LINE:
        return _gapBetweenTextAndLine;
    case Pid::END_FONT_FACE:
        return _endFontFamily;
    case Pid::END_FONT_SIZE:
        return _endFontSize;
    case Pid::END_FONT_STYLE:
        return int(_endFontStyle);
    case Pid::END_TEXT_OFFSET:
        return _endTextOffset;
    case Pid::LINE_VISIBLE:
        return lineVisible();
    case Pid::TEXT_SIZE_SPATIUM_DEPENDENT:
        return textSizeSpatiumDependent();
    default:
        return SLine::getProperty(id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool TextLineBase::setProperty(Pid id, const PropertyValue& v)
{
    switch (id) {
    //  Text (global)
    /*case Pid::TEXT_STYLE:
        initTextStyleType(v.value<TextStyleType>());
        break;*/
    case Pid::FONT_FACE:
        setProperty(Pid::BEGIN_FONT_FACE, v);
        setProperty(Pid::CONTINUE_FONT_FACE, v);
        setProperty(Pid::END_FONT_FACE, v);
        break;
    case Pid::FONT_SIZE:
        setProperty(Pid::BEGIN_FONT_SIZE, v);
        setProperty(Pid::CONTINUE_FONT_SIZE, v);
        setProperty(Pid::END_FONT_SIZE, v);
        break;
    case Pid::FONT_STYLE:
        setProperty(Pid::BEGIN_FONT_STYLE, v);
        setProperty(Pid::CONTINUE_FONT_STYLE, v);
        setProperty(Pid::END_FONT_STYLE, v);
        break;
    /*case Pid::TEXT_LINE_SPACING:
        setTextLineSpacing(v.toReal());
        break;*/
    /*case Pid::FRAME_TYPE:
        setFrameType(FrameType(v.toInt()));
        break;
    case Pid::FRAME_WIDTH:
        setFrameWidth(v.value<Spatium>());
        break;
    case Pid::FRAME_PADDING:
        setPaddingWidth(v.value<Spatium>());
        break;
    case Pid::FRAME_ROUND:
        setFrameRound(v.toInt());
        break;
    case Pid::FRAME_FG_COLOR:
        setFrameColor(v.value<Color>());
        break;
    case Pid::FRAME_BG_COLOR:
        setBgColor(v.value<Color>());
        break;*/
    /*case Pid::TEXT:
        setBeginText(v.value<String>());
        setContinueText(v.value<String>());
        setEndText(v.value<String>());
        break;*/
    case Pid::ALIGN:
        setProperty(Pid::BEGIN_TEXT_ALIGN, v);
        setProperty(Pid::CONTINUE_TEXT_ALIGN, v);
        setProperty(Pid::END_TEXT_ALIGN, v);
        break;
    /*case Pid::TEXT_SCRIPT_ALIGN:
        m_cursor->setFormat(FormatId::Valign, v.toInt());
        break;*/
    /*case Pid::TEXT_LINKED_TO_MASTER:
        if (isTextLinkedToMaster() == v.toBool()) {
            break;
        }
        if (!isTextLinkedToMaster()) {
            relinkPropertiesToMaster(PropertyGroup::TEXT);
        }
        setTextLinkedToMaster(v.toBool());
        break;*/
    //  Text (local)
    case Pid::BEGIN_TEXT_ALIGN:
        _beginTextAlign = v.value<Align>();
        break;
    case Pid::CONTINUE_TEXT_ALIGN:
        _continueTextAlign = v.value<Align>();
        break;
    case Pid::END_TEXT_ALIGN:
        _endTextAlign = v.value<Align>();
        break;
    case Pid::BEGIN_TEXT_PLACE:
        _beginTextPlace = v.value<TextPlace>();
        break;
    case Pid::CONTINUE_TEXT_PLACE:
        _continueTextPlace = v.value<TextPlace>();
        break;
    case Pid::END_TEXT_PLACE:
        _endTextPlace = v.value<TextPlace>();
        break;
    case Pid::BEGIN_TEXT:
        setBeginText(v.value<String>());
        break;
    case Pid::BEGIN_TEXT_OFFSET:
        setBeginTextOffset(v.value<PointF>());
        break;
    case Pid::CONTINUE_TEXT:
        setContinueText(v.value<String>());
        break;
    case Pid::CONTINUE_TEXT_OFFSET:
        setContinueTextOffset(v.value<PointF>());
        break;
    case Pid::END_TEXT_OFFSET:
        setEndTextOffset(v.value<PointF>());
        break;
    case Pid::END_TEXT:
        setEndText(v.value<String>());
        break;
    case Pid::BEGIN_FONT_FACE:
        setBeginFontFamily(v.value<String>());
        break;
    case Pid::BEGIN_FONT_SIZE:
        setBeginFontSize(v.toReal());
        break;
    case Pid::BEGIN_FONT_STYLE:
        setBeginFontStyle(FontStyle(v.toInt()));
        break;
    case Pid::CONTINUE_FONT_FACE:
        setContinueFontFamily(v.value<String>());
        break;
    case Pid::CONTINUE_FONT_SIZE:
        setContinueFontSize(v.toReal());
        break;
    case Pid::CONTINUE_FONT_STYLE:
        setContinueFontStyle(FontStyle(v.toInt()));
        break;
    case Pid::END_FONT_FACE:
        setEndFontFamily(v.value<String>());
        break;
    case Pid::END_FONT_SIZE:
        setEndFontSize(v.toReal());
        break;
    case Pid::END_FONT_STYLE:
        setEndFontStyle(FontStyle(v.toInt()));
        break;
    case Pid::TEXT_SIZE_SPATIUM_DEPENDENT:
        setTextSizeSpatiumDependent(v.toBool());
        break;
    // Line
    case Pid::BEGIN_HOOK_HEIGHT:
        _beginHookHeight = v.value<Spatium>();
        break;
    case Pid::END_HOOK_HEIGHT:
        _endHookHeight = v.value<Spatium>();
        break;
    case Pid::BEGIN_HOOK_TYPE:
        _beginHookType = v.value<HookType>();
        break;
    case Pid::END_HOOK_TYPE:
        _endHookType = v.value<HookType>();
        break;
    case Pid::GAP_BETWEEN_TEXT_AND_LINE:
        _gapBetweenTextAndLine = v.value<Spatium>();
        break;
    case Pid::LINE_VISIBLE:
        setLineVisible(v.toBool());
        break;
    default:
        return SLine::setProperty(id, v);
    }
    triggerLayout();
    return true;
}

mu::engraving::PropertyValue TextLineBase::propertyDefault(Pid propertyId) const
{
    switch (propertyId) {
    case Pid::GAP_BETWEEN_TEXT_AND_LINE:
        return Spatium(0.5);
    default:
        return SLine::propertyDefault(propertyId);
    }
}
}
