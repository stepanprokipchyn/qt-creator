/****************************************************************************
**
** Copyright (C) 2016 Orgad Shaneh <orgads@gmail.com>.
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

#include "gitgrep.h"
#include "gitclient.h"
#include "gitplugin.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/progressmanager/progressmanager.h>
#include <coreplugin/vcsmanager.h>
#include <texteditor/findinfiles.h>
#include <vcsbase/vcscommand.h>
#include <vcsbase/vcsbaseconstants.h>

#include <utils/fancylineedit.h>
#include <utils/filesearch.h>
#include <utils/fileutils.h>
#include <utils/qtcassert.h>
#include <utils/runextensions.h>
#include <utils/synchronousprocess.h>
#include <utils/textfileformat.h>

#include <QCheckBox>
#include <QFuture>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QRegularExpressionValidator>
#include <QScopedPointer>
#include <QSettings>
#include <QTextStream>

namespace Git {
namespace Internal {

class GitGrepParameters
{
public:
    QString ref;
    bool isEnabled = false;
};

using namespace Core;
using namespace Utils;
using VcsBase::VcsCommand;

namespace {

const char EnableGitGrep[] = "EnableGitGrep";
const char GitGrepRef[] = "GitGrepRef";

class GitGrepRunner : public QObject
{
    using FutureInterfaceType = QFutureInterface<FileSearchResultList>;

public:
    GitGrepRunner(FutureInterfaceType &fi,
                  const TextEditor::FileFindParameters &parameters) :
        m_fi(fi),
        m_parameters(parameters)
    {
        m_directory = parameters.additionalParameters.toString();
    }

    void processLine(const QString &line, FileSearchResultList *resultList) const
    {
        if (line.isEmpty())
            return;
        static const QLatin1String boldRed("\x1b[1;31m");
        static const QLatin1String resetColor("\x1b[m");
        FileSearchResult single;
        const int lineSeparator = line.indexOf(QChar::Null);
        QString filePath = line.left(lineSeparator);
        if (!m_ref.isEmpty() && filePath.startsWith(m_ref))
            filePath.remove(0, m_ref.length());
        single.fileName = m_directory + QLatin1Char('/') + filePath;
        const int textSeparator = line.indexOf(QChar::Null, lineSeparator + 1);
        single.lineNumber = line.mid(lineSeparator + 1, textSeparator - lineSeparator - 1).toInt();
        QString text = line.mid(textSeparator + 1);
        QVector<QPair<int, int>> matches;
        for (;;) {
            const int matchStart = text.indexOf(boldRed);
            if (matchStart == -1)
                break;
            const int matchTextStart = matchStart + boldRed.size();
            const int matchEnd = text.indexOf(resetColor, matchTextStart);
            QTC_ASSERT(matchEnd != -1, break);
            const int matchLength = matchEnd - matchTextStart;
            matches.append(qMakePair(matchStart, matchLength));
            text = text.left(matchStart) + text.mid(matchTextStart, matchLength)
                    + text.mid(matchEnd + resetColor.size());
        }
        single.matchingLine = text;
        foreach (auto match, matches) {
            single.matchStart = match.first;
            single.matchLength = match.second;
            resultList->append(single);
        }
    }

    void read(const QString &text)
    {
        FileSearchResultList resultList;
        QString t = text;
        QTextStream stream(&t);
        while (!stream.atEnd() && !m_fi.isCanceled())
            processLine(stream.readLine(), &resultList);
        if (!resultList.isEmpty())
            m_fi.reportResult(resultList);
    }

    void exec()
    {
        QStringList arguments;
        arguments << QLatin1String("-c") << QLatin1String("color.grep.match=bold red")
                  << QLatin1String("grep") << QLatin1String("-zn")
                  << QLatin1String("--color=always");
        if (!(m_parameters.flags & FindCaseSensitively))
            arguments << QLatin1String("-i");
        if (m_parameters.flags & FindWholeWords)
            arguments << QLatin1String("-w");
        if (m_parameters.flags & FindRegularExpression)
            arguments << QLatin1String("-P");
        else
            arguments << QLatin1String("-F");
        arguments << m_parameters.text;
        GitGrepParameters params = m_parameters.extensionParameters.value<GitGrepParameters>();
        if (!params.ref.isEmpty()) {
            arguments << params.ref;
            m_ref = params.ref + QLatin1Char(':');
        }
        arguments << QLatin1String("--") << m_parameters.nameFilters;
        QScopedPointer<VcsCommand> command(GitPlugin::client()->createCommand(m_directory));
        command->addFlags(VcsCommand::SilentOutput);
        command->setProgressiveOutput(true);
        QFutureWatcher<FileSearchResultList> watcher;
        watcher.setFuture(m_fi.future());
        connect(&watcher, &QFutureWatcher<FileSearchResultList>::canceled,
                command.data(), &VcsCommand::cancel);
        connect(command.data(), &VcsCommand::stdOutText, this, &GitGrepRunner::read);
        SynchronousProcessResponse resp = command->runCommand(GitPlugin::client()->vcsBinary(), arguments, 0);
        switch (resp.result) {
        case SynchronousProcessResponse::TerminatedAbnormally:
        case SynchronousProcessResponse::StartFailed:
        case SynchronousProcessResponse::Hang:
            m_fi.reportCanceled();
            break;
        case SynchronousProcessResponse::Finished:
        case SynchronousProcessResponse::FinishedError:
            // When no results are found, git-grep exits with non-zero status.
            // Do not consider this as an error.
            break;
        }
    }

    static void run(QFutureInterface<FileSearchResultList> &fi,
                    TextEditor::FileFindParameters parameters)
    {
        GitGrepRunner runner(fi, parameters);
        Core::ProgressTimer progress(fi, 20);
        runner.exec();
    }

private:
    FutureInterfaceType m_fi;
    QString m_directory;
    QString m_ref;
    const TextEditor::FileFindParameters &m_parameters;
};

} // namespace

static bool validateDirectory(const QString &path)
{
    static IVersionControl *gitVc = VcsManager::versionControl(VcsBase::Constants::VCS_ID_GIT);
    QTC_ASSERT(gitVc, return false);
    return gitVc == VcsManager::findVersionControlForDirectory(path, 0);
}

GitGrep::GitGrep()
{
    m_widget = new QWidget;
    auto layout = new QHBoxLayout(m_widget);
    layout->setMargin(0);
    m_enabledCheckBox = new QCheckBox(tr("&Use Git Grep"));
    m_enabledCheckBox->setToolTip(tr("Use Git Grep for searching. This includes only files "
                                     "that are managed by Git."));
    layout->addWidget(m_enabledCheckBox);
    m_treeLineEdit = new FancyLineEdit;
    m_treeLineEdit->setPlaceholderText(
                tr("Tree: add reference here or leave empty to search through the file system)"));
    m_treeLineEdit->setToolTip(
                tr("Reference can be HEAD, tag, local or remote branch, or a commit hash."));
    const QRegularExpression refExpression(QLatin1String("[\\w/]*"));
    m_treeLineEdit->setValidator(new QRegularExpressionValidator(refExpression, this));
    layout->addWidget(m_treeLineEdit);
    TextEditor::FindInFiles *findInFiles = TextEditor::FindInFiles::instance();
    QTC_ASSERT(findInFiles, return);
    connect(findInFiles, &TextEditor::FindInFiles::pathChanged,
            m_widget, [this](const QString &path) {
        m_widget->setEnabled(validateDirectory(path));
    });
    findInFiles->setFindExtension(this);
}

GitGrep::~GitGrep()
{
    delete m_widget;
}

QString GitGrep::title() const
{
    return tr("Git Grep");
}

QString GitGrep::toolTip() const
{
    const QString ref = m_treeLineEdit->text();
    if (!ref.isEmpty())
        return tr("Ref: %1\n%2").arg(ref);
    return QLatin1String("%1");
}

QWidget *GitGrep::widget() const
{
    return m_widget;
}

bool GitGrep::isEnabled() const
{
    return m_widget->isEnabled() && m_enabledCheckBox->isChecked();
}

bool GitGrep::isEnabled(const TextEditor::FileFindParameters &parameters) const
{
    return parameters.extensionParameters.value<GitGrepParameters>().isEnabled;
}

QVariant GitGrep::parameters() const
{
    GitGrepParameters params;
    params.isEnabled = isEnabled();
    params.ref = m_treeLineEdit->text();
    return qVariantFromValue(params);
}

void GitGrep::readSettings(QSettings *settings)
{
    m_enabledCheckBox->setChecked(settings->value(QLatin1String(EnableGitGrep), false).toBool());
    m_treeLineEdit->setText(settings->value(QLatin1String(GitGrepRef)).toString());
}

void GitGrep::writeSettings(QSettings *settings) const
{
    settings->setValue(QLatin1String(EnableGitGrep), m_enabledCheckBox->isChecked());
    settings->setValue(QLatin1String(GitGrepRef), m_treeLineEdit->text());
}

QFuture<FileSearchResultList> GitGrep::executeSearch(
        const TextEditor::FileFindParameters &parameters)
{
    return Utils::runAsync(GitGrepRunner::run, parameters);
}

IEditor *GitGrep::openEditor(const SearchResultItem &item,
                             const TextEditor::FileFindParameters &parameters)
{
    GitGrepParameters params = parameters.extensionParameters.value<GitGrepParameters>();
    if (!params.isEnabled || params.ref.isEmpty() || item.path.isEmpty())
        return nullptr;
    const QString path = QDir::fromNativeSeparators(item.path.first());
    QByteArray content;
    const QString topLevel = parameters.additionalParameters.toString();
    const QString relativePath = QDir(topLevel).relativeFilePath(path);
    if (!GitPlugin::client()->synchronousShow(topLevel, params.ref + QLatin1String(":./") + relativePath,
                                              &content, nullptr)) {
        return nullptr;
    }
    if (content.isEmpty())
        return nullptr;
    QByteArray fileContent;
    if (TextFileFormat::readFileUTF8(path, 0, &fileContent, 0) == TextFileFormat::ReadSuccess) {
        if (fileContent == content)
            return nullptr; // open the file for read/write
    }
    QString title = tr("Git Show %1:%2").arg(params.ref).arg(relativePath);
    IEditor *editor = EditorManager::openEditorWithContents(Id(), &title, content, title,
                                                            EditorManager::DoNotSwitchToDesignMode);
    editor->gotoLine(item.lineNumber, item.textMarkPos);
    editor->document()->setTemporary(true);
    return editor;
}

} // Internal
} // Git

Q_DECLARE_METATYPE(Git::Internal::GitGrepParameters)
