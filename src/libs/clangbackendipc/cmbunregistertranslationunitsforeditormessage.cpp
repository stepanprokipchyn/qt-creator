/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "cmbunregistertranslationunitsforeditormessage.h"

#ifdef CLANGBACKEND_TESTS
#include <gtest/gtest-printers.h>
#endif

#include <QDataStream>
#include <QDebug>

namespace ClangBackEnd {


UnregisterTranslationUnitsForEditorMessage::UnregisterTranslationUnitsForEditorMessage(const QVector<FileContainer> &fileContainers)
    : fileContainers_(fileContainers)
{
}

const QVector<FileContainer> &UnregisterTranslationUnitsForEditorMessage::fileContainers() const
{
    return fileContainers_;
}

QDataStream &operator<<(QDataStream &out, const UnregisterTranslationUnitsForEditorMessage &message)
{
    out << message.fileContainers_;

    return out;
}

QDataStream &operator>>(QDataStream &in, UnregisterTranslationUnitsForEditorMessage &message)
{
    in >> message.fileContainers_;

    return in;
}

bool operator==(const UnregisterTranslationUnitsForEditorMessage &first, const UnregisterTranslationUnitsForEditorMessage &second)
{
    return first.fileContainers_ == second.fileContainers_;
}

QDebug operator<<(QDebug debug, const UnregisterTranslationUnitsForEditorMessage &message)
{
    debug.nospace() << "UnregisterTranslationUnitsForEditorMessage(";

    for (const FileContainer &fileContainer : message.fileContainers())
        debug.nospace() << fileContainer << ", ";

    debug.nospace() << ")";

    return debug;
}

#ifdef CLANGBACKEND_TESTS
void PrintTo(const UnregisterTranslationUnitsForEditorMessage &message, ::std::ostream* os)
{
    *os << "UnregisterTranslationUnitsForEditorMessage(";

    for (const FileContainer &fileContainer : message.fileContainers())
        *os << ::testing::PrintToString(fileContainer) << ", ";

    *os << ")";
}
#endif


} // namespace ClangBackEnd

