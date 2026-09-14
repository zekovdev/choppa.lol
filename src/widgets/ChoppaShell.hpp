// SPDX-License-Identifier: MIT
#pragma once

#include <pajlada/signals/signalholder.hpp>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace chatterino {
class Window;
class SplitNotebook;

// The desktop shell owns navigation; the notebook owns chat pages and their
// persistence. Keeping these separate preserves the existing Twitch/7TV engine.
class ChoppaShell : public QWidget
{
    Q_OBJECT
public:
    ChoppaShell(Window *window, SplitNotebook *notebook);
    void focusNavigation();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void scheduleRefresh();
    void refreshNavigation();
    void updateAccount();
    void addChannel(bool newWorkspace);
    void showWorkspaceMenu(const QPoint &position);
    void updateNavigationLayout();

    Window *window_;
    SplitNotebook *notebook_;
    QWidget *sidebar_;
    QWidget *compactNavigation_;
    QLineEdit *filter_;
    QListWidget *workspaces_;
    QLabel *title_;
    QLabel *subtitle_;
    QLabel *emptySearch_;
    QPushButton *account_;
    QPushButton *streamer_;
    bool refreshPending_ = false;
    bool sidebarExpanded_ = false;
    pajlada::Signals::SignalHolder signals_;
};
}  // namespace chatterino
