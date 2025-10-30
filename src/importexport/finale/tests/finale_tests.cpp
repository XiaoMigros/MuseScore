/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited
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

#include <gtest/gtest.h>

#include "engraving/engravingerrors.h"
#include "engraving/dom/masterscore.h"
// for edit tests
#include "engraving/dom/measure.h"
#include "engraving/dom/segment.h"

#include "importexport/finale/internal/importfinale.h"

#include "engraving/tests/utils/scorerw.h"
#include "engraving/tests/utils/scorecomp.h"

#include "io/fileinfo.h"

using namespace mu;
using namespace muse;
using namespace mu::iex::finale;
using namespace mu::engraving;

static const String FINALE_IO_DATA_DIR("data/");

static const std::string MODULE_NAME("iex_finale");

class Finale_Tests : public ::testing::Test
{
public:
    void finaleImportTestRef(const char* file);
    void finaleImportTestEdit(const char* file);
    void enigmaXmlImportTestRef(const char* file);

    MasterScore* readScore(const String& fileName, bool isAbsolutePath = false);
};

//---------------------------------------------------------
//   fixupScore -- do required fixups after reading/importing score
//---------------------------------------------------------
static void fixupScore(MasterScore* score) // probably not needed
{
    score->connectTies();
    score->masterScore()->rebuildMidiMapping();
    score->setSaved(false);
}

MasterScore* Finale_Tests::readScore(const String& fileName, bool isAbsolutePath)
{
    String suffix = io::FileInfo::suffix(fileName);

    auto loadMusx = [](MasterScore* score, const muse::io::path_t& path) -> engraving::Err {
        return importMusx(score, path.toQString());
    };

    auto loadEnigmaXml = [](MasterScore* score, const muse::io::path_t& path) -> engraving::Err {
        return importEnigmaXml(score, path.toQString());
    };

    ScoreRW::ImportFunc importFunc = nullptr;
    if (suffix == u"musx") {
        importFunc = loadMusx;
    } else if (suffix == u"enigmaxml") {
        importFunc = loadEnigmaXml;
    }

    MasterScore* score = ScoreRW::readScore(fileName, isAbsolutePath, importFunc);
    return score;
}

//---------------------------------------------------------
//   finaleImportTestRef
//   read a Musx file, write to a new MuseScore mscx file
//   and verify against a MuseScore mscx reference file
//---------------------------------------------------------

void Finale_Tests::finaleImportTestRef(const char* file)
{
    MScore::debugMode = false;

    String fileName = String::fromUtf8(file);
    MasterScore* score = readScore(FINALE_IO_DATA_DIR + fileName + u".musx");
    EXPECT_TRUE(score);
    // fixupScore(score);
    score->doLayout();
    // EXPECT_TRUE(ScoreComp::saveCompareScore(score, fileName + u".mscx", FINALE_IO_DATA_DIR + fileName + u"_ref.mscx"));
    delete score;
}

void Finale_Tests::finaleImportTestEdit(const char* file)
{
    MScore::debugMode = false;

    String fileName = String::fromUtf8(file);
    MasterScore* score = readScore(FINALE_IO_DATA_DIR + fileName + u".musx");
    EXPECT_TRUE(score);
    // fixupScore(score);
    score->doLayout();
    if (Measure* m = score->tick2measure(Fraction(0, 1))) {
        Segment* s = m->first(SegmentType::ChordRest);
        if (s->element(0)) {
            score->startCmd(TranslatableString::untranslatable("Import Finale edit tests"));
            s->element(0)->undoChangeProperty(Pid::OFFSET, PointF(2, 3));
            score->endCmd();
        }
    }

    EXPECT_TRUE(score);
    delete score;
}

//---------------------------------------------------------
//   enigmaXmlImportTestRef
//   read an .enigmaxml file, write to a new MuseScore mscx file
//   and verify against a MuseScore mscx reference file
//---------------------------------------------------------

void Finale_Tests::enigmaXmlImportTestRef(const char* file)
{
    MScore::debugMode = false;

    String fileName = String::fromUtf8(file);
    MasterScore* score = readScore(FINALE_IO_DATA_DIR + fileName + u".enigmaxml");
    EXPECT_TRUE(score);
    fixupScore(score);
    score->doLayout();
    // EXPECT_TRUE(ScoreComp::saveCompareScore(score, fileName + u".mscx", FINALE_IO_DATA_DIR + fileName + u"_ref.mscx"));
    delete score;
}

#define MUSX_IMPORT_TEST(name) \
    TEST_F (Finale_Tests, name) { \
        finaleImportTestRef(#name); \
    }

#define MUSX_IMPORT_TEST_EDIT(name) \
    TEST_F (Finale_Tests, edit_##name) { \
        finaleImportTestEdit(#name); \
    }

#define MUSX_IMPORT_TEST_DISABLED(name) \
    TEST_F(Finale_Tests, DISABLED_##name) { \
        finaleImportTestRef(#name); \
    }

MUSX_IMPORT_TEST(texts)
MUSX_IMPORT_TEST_EDIT(texts)
MUSX_IMPORT_TEST_EDIT(shapess)
MUSX_IMPORT_TEST(smartShapes1)
MUSX_IMPORT_TEST(stafflists)
MUSX_IMPORT_TEST(multistaffInst)
MUSX_IMPORT_TEST_DISABLED(onePartNoMeasures)
MUSX_IMPORT_TEST(grandStaffPartNoMeasures)
MUSX_IMPORT_TEST_DISABLED(twoPartsNoMeasures)
MUSX_IMPORT_TEST(twoPartsNoMeasuresWithBracket)
MUSX_IMPORT_TEST_DISABLED(onePartOneMeasure)
MUSX_IMPORT_TEST(onePartOneMeasureWithNotes)
MUSX_IMPORT_TEST(oneMeasureCrossStaff)
MUSX_IMPORT_TEST(onePartOneMeasureWithTuplets)
MUSX_IMPORT_TEST(onePartOneMeasureWithNestedTuplets)
MUSX_IMPORT_TEST(oneMeasureWithTies)
MUSX_IMPORT_TEST(v1v2Ties)
MUSX_IMPORT_TEST(v1v2Ties2)

#undef MUSX_IMPORT_TEST
#undef MUSX_IMPORT_TEST_EDIT
#undef MUSX_IMPORT_TEST_DISABLED
