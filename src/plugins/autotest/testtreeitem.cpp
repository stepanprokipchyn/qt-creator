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

#include "autotestconstants.h"
#include "testtreeitem.h"

#include <utils/qtcassert.h>

#include <QIcon>

#include <texteditor/texteditor.h>

namespace Autotest {
namespace Internal {

TestTreeItem::TestTreeItem(const QString &name, const QString &filePath, Type type)
    : TreeItem( { name } ),
      m_name(name),
      m_filePath(filePath),
      m_type(type),
      m_line(0),
      m_state(Enabled),
      m_markedForRemoval(false)
{
    switch (m_type) {
    case TestClass:
    case TestFunction:
    case GTestCase:
    case GTestCaseParameterized:
    case GTestName:
        m_checked = Qt::Checked;
        break;
    default:
        m_checked = Qt::Unchecked;
    }
}

TestTreeItem::~TestTreeItem()
{
    removeChildren();
}

TestTreeItem::TestTreeItem(const TestTreeItem &other)
    : TreeItem( { other.m_name } ),
      m_name(other.m_name),
      m_filePath(other.m_filePath),
      m_checked(other.m_checked),
      m_type(other.m_type),
      m_line(other.m_line),
      m_column(other.m_column),
      m_mainFile(other.m_mainFile),
      m_referencingFile(other.m_referencingFile),
      m_state(other.m_state),
      m_markedForRemoval(other.m_markedForRemoval)
{
    for (int row = 0, count = other.childCount(); row < count; ++row)
        appendChild(new TestTreeItem(*other.childItem(row)));
}

static QIcon testTreeIcon(TestTreeItem::Type type)
{
    static QIcon icons[] = {
        QIcon(),
        QIcon(QLatin1String(":/images/class.png")),
        QIcon(QLatin1String(":/images/func.png")),
        QIcon(QLatin1String(":/images/data.png"))
    };
    if (type == TestTreeItem::GTestCase || type == TestTreeItem::GTestCaseParameterized)
        return icons[1];

    if (int(type) >= int(sizeof icons / sizeof *icons))
        return icons[2];
    return icons[type];
}

QVariant TestTreeItem::data(int /*column*/, int role) const
{
    switch (role) {
    case Qt::DisplayRole:
        if (m_type == Root && childCount() == 0)
            return QString(m_name + QObject::tr(" (none)"));
        else if (m_name.isEmpty())
            return QObject::tr(Constants::UNNAMED_QUICKTESTS);
        else if (m_type == GTestCaseParameterized)
            return QString(m_name + QObject::tr(" [parameterized]"));
        else
            return m_name;
    case Qt::ToolTipRole:
        if (m_type == TestClass && m_name.isEmpty()) {
            return QObject::tr("<p>Give all test cases a name to ensure correct behavior "
                               "when running test cases and to be able to select them.</p>");
        }
        return m_filePath;
    case Qt::DecorationRole:
        return testTreeIcon(m_type);
    case Qt::CheckStateRole:
        switch (m_type) {
        case Root:
        case TestDataFunction:
        case TestSpecialFunction:
        case TestDataTag:
            return QVariant();
        case TestClass:
        case GTestCase:
        case GTestCaseParameterized:
            return m_name.isEmpty() ? QVariant() : checked();
        case TestFunction:
        case GTestName:
            if (parentItem() && parentItem()->name().isEmpty())
                return QVariant();
            return checked();
        default:
            return checked();
        }
    case LinkRole: {
        QVariant itemLink;
        itemLink.setValue(TextEditor::TextEditorWidget::Link(m_filePath, m_line, m_column));
        return itemLink;
    }
    case ItalicRole:
        switch (m_type) {
        case TestDataFunction:
        case TestSpecialFunction:
            return true;
        case TestClass:
            return m_name.isEmpty();
        case TestFunction:
            return parentItem() ? parentItem()->name().isEmpty() : false;
        default:
            return false;
        }
    case TypeRole:
        return m_type;
    case StateRole:
        return (int)m_state;
    }
    return QVariant();
}

bool TestTreeItem::setData(int /*column*/, const QVariant &data, int role)
{
    if (role == Qt::CheckStateRole) {
        Qt::CheckState old = checked();
        setChecked((Qt::CheckState)data.toInt());
        return checked() != old;
    }
    return false;
}

bool TestTreeItem::modifyContent(const TestTreeItem *modified)
{
    bool hasBeenModified = false;
    if (m_filePath != modified->m_filePath) {
        m_filePath = modified->m_filePath;
        hasBeenModified = true;
    }
    if (m_name != modified->m_name) {
        m_name = modified->m_name;
        hasBeenModified = true;
    }
    if (m_line != modified->m_line) {
        m_line = modified->m_line;
        hasBeenModified = true;
    }
    if (m_mainFile != modified->m_mainFile) {
        m_mainFile = modified->m_mainFile;
        hasBeenModified = true;
    }
    if (m_referencingFile != modified->m_referencingFile) {
        m_referencingFile = modified->m_referencingFile;
        hasBeenModified = true;
    }
    if (m_type != modified->m_type) {
        m_type = modified->m_type;
        hasBeenModified = true;
    }
    if (m_state != modified->m_state) {
        m_state = modified->m_state;
        hasBeenModified = true;
    }
    if (m_markedForRemoval != modified->m_markedForRemoval)
        m_markedForRemoval = modified->m_markedForRemoval;
    return hasBeenModified;
}

void TestTreeItem::setChecked(const Qt::CheckState checkState)
{
    switch (m_type) {
    case TestFunction:
    case GTestName: {
        m_checked = (checkState == Qt::Unchecked ? Qt::Unchecked : Qt::Checked);
        parentItem()->revalidateCheckState();
        break;
    }
    case TestClass:
    case GTestCase:
    case GTestCaseParameterized: {
        Qt::CheckState usedState = (checkState == Qt::Unchecked ? Qt::Unchecked : Qt::Checked);
        for (int row = 0, count = childCount(); row < count; ++row)
            childItem(row)->setChecked(usedState);
        m_checked = usedState;
    }
    default:
        return;
    }
}

Qt::CheckState TestTreeItem::checked() const
{
    switch (m_type) {
    case TestClass:
    case TestFunction:
    case GTestCase:
    case GTestCaseParameterized:
    case GTestName:
        return m_checked;
    case TestDataFunction:
    case TestSpecialFunction:
        return Qt::Unchecked;
    default:
        if (parent())
            return parentItem()->m_checked;
    }
    return Qt::Unchecked;
}

void TestTreeItem::markForRemoval(bool mark)
{
    m_markedForRemoval = mark;
}

void TestTreeItem::markForRemovalRecursively(bool mark)
{
    m_markedForRemoval = mark;
    for (int row = 0, count = childCount(); row < count; ++row)
        childItem(row)->markForRemovalRecursively(mark);
}

TestTreeItem *TestTreeItem::parentItem() const
{
    return static_cast<TestTreeItem *>(parent());
}

TestTreeItem *TestTreeItem::childItem(int row) const
{
    return static_cast<TestTreeItem *>(child(row));
}

void TestTreeItem::revalidateCheckState()
{
    if (childCount() == 0)
        return;
    bool foundChecked = false;
    bool foundUnchecked = false;
    for (int row = 0, count = childCount(); row < count; ++row) {
        TestTreeItem *child = childItem(row);
        switch (child->type()) {
        case TestDataFunction:
        case TestSpecialFunction:
            continue;
        default:
            break;
        }

        foundChecked |= (child->checked() != Qt::Unchecked);
        foundUnchecked |= (child->checked() == Qt::Unchecked);
        if (foundChecked && foundUnchecked) {
            m_checked = Qt::PartiallyChecked;
            return;
        }
    }
    m_checked = (foundUnchecked ? Qt::Unchecked : Qt::Checked);
}

} // namespace Internal
} // namespace Autotest
