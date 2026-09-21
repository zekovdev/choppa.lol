// SPDX-License-Identifier: MIT
#include "widgets/ChoppaShell.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/dialogs/SettingsDialog.hpp"
#include "widgets/helper/NotebookTab.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/Window.hpp"

#include <QApplication>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QRegion>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QWindow>

namespace chatterino {
namespace {
constexpr int LiveRole = Qt::UserRole + 1;
constexpr int HighlightRole = Qt::UserRole + 2;

class WorkspaceDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &) const override
    {
        return option.widget && option.widget->property("compact").toBool()
                   ? QSize(84, 28)
                   : QSize(170, 34);
    }
    void paint(QPainter *p, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setRenderHint(QPainter::TextAntialiasing);
        const auto rect = option.rect.adjusted(0, 2, -1, -2);
        const bool selected = option.state & QStyle::State_Selected;
        p->setPen(Qt::NoPen);
        if (selected || (option.state & QStyle::State_MouseOver))
        {
            p->setBrush(QColor(selected ? "#202020" : "#292929"));
            p->drawRoundedRect(rect, 8, 8);
        }
        const auto highlight =
            static_cast<HighlightState>(index.data(HighlightRole).toInt());
        auto font = option.font;
        font.setWeight(QFont::Bold);
        p->setFont(font);
        p->setPen(QColor(selected || highlight != HighlightState::None
                             ? "#ffffff"
                             : "#999999"));
        const bool compact =
            option.widget && option.widget->property("compact").toBool();
        const auto textRect = rect.adjusted(compact ? 8 : 12, 0, -16, 0);
        p->drawText(
            textRect, Qt::AlignVCenter,
            QFontMetrics(font).elidedText(index.data().toString(),
                                          Qt::ElideRight, textRect.width()));
        if (index.data(LiveRole).toBool())
        {
            p->setPen(Qt::NoPen);
            p->setBrush(QColor("#ef5261"));
            p->drawEllipse(QPointF(rect.right() - 7, rect.center().y()), 3, 3);
        }
        if (highlight == HighlightState::Highlighted)
        {
            p->setPen(QPen(QColor("#e8ad58"), 2));
            p->drawLine(rect.bottomLeft() + QPoint(8, -1),
                        rect.bottomRight() + QPoint(-8, -1));
        }
        if (option.state & QStyle::State_HasFocus)
        {
            p->setPen(QColor("#888888"));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(rect.adjusted(1, 1, -1, -1), 8, 8);
        }
        p->restore();
    }
};

class ChatTabList : public QListWidget
{
public:
    using QListWidget::QListWidget;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        QListWidget::mousePressEvent(event);
        if (event->button() == Qt::LeftButton)
        {
            this->draggedRow_ = this->indexAt(event->position().toPoint()).row();
            this->dragStart_ = event->position().toPoint();
            this->dragging_ = false;
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!(event->buttons() & Qt::LeftButton) || this->draggedRow_ < 0)
        {
            QListWidget::mouseMoveEvent(event);
            return;
        }

        const auto position = event->position().toPoint();
        if (!this->dragging_ &&
            (position - this->dragStart_).manhattanLength() <
                QApplication::startDragDistance())
        {
            return;
        }

        this->dragging_ = true;
        this->viewport()->setCursor(Qt::ClosedHandCursor);
        const int target = this->indexAt(position).row();
        if (target >= 0 && target != this->draggedRow_)
        {
            const int destination =
                target > this->draggedRow_ ? target + 1 : target;
            if (this->model()->moveRow(QModelIndex(), this->draggedRow_,
                                       QModelIndex(), destination))
            {
                this->draggedRow_ = target;
                this->setCurrentRow(target);
            }
        }
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        this->viewport()->unsetCursor();
        this->draggedRow_ = -1;
        this->dragging_ = false;
        QListWidget::mouseReleaseEvent(event);
    }

private:
    QPoint dragStart_;
    int draggedRow_ = -1;
    bool dragging_ = false;
};

QPushButton *button(const QString &text, const QString &name = "nav")
{
    auto *result = new QPushButton(text);
    result->setObjectName(name);
    result->setCursor(Qt::PointingHandCursor);
    return result;
}

QLabel *label(const QString &text, const QString &name)
{
    auto *result = new QLabel(text);
    result->setObjectName(name);
    result->setTextFormat(Qt::PlainText);
    return result;
}
}  // namespace

ChoppaShell::ChoppaShell(Window *window, SplitNotebook *notebook)
    : QWidget(window)
    , window_(window)
    , notebook_(notebook)
{
    this->setObjectName("choppaShell");
    this->setAttribute(Qt::WA_StyledBackground);
    QFile stylesheet(":/choppa/shell.qss");
    if (stylesheet.open(QIODevice::ReadOnly))
    {
        this->setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
    }
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *body = new QHBoxLayout;
    body->setContentsMargins(6, 0, 6, 6);
    body->setSpacing(8);
    root->addLayout(body, 1);
    this->sidebar_ = new QWidget(this);
    this->sidebar_->setObjectName("choppaSidebar");
    this->sidebar_->setFixedWidth(176);
    auto *nav = new QVBoxLayout(this->sidebar_);
    nav->setContentsMargins(0, 4, 0, 0);
    nav->setSpacing(4);
    this->filter_ = new QLineEdit;
    this->filter_->setPlaceholderText(tr("Find a chat..."));
    this->filter_->setAccessibleName(tr("Filter chats"));
    this->filter_->setClearButtonEnabled(true);
    nav->addWidget(this->filter_);
    nav->addWidget(label(tr("CHATS"), "section"));
    this->workspaces_ = new ChatTabList;
    this->workspaces_->setObjectName("choppaChatTabs");
    this->workspaces_->setAccessibleName(tr("Chat tabs"));
    this->workspaces_->setMouseTracking(true);
    this->workspaces_->setDragDropMode(QAbstractItemView::InternalMove);
    this->workspaces_->setDefaultDropAction(Qt::MoveAction);
    this->workspaces_->setDragDropOverwriteMode(false);
    this->workspaces_->setDropIndicatorShown(true);
    this->workspaces_->setEditTriggers(QAbstractItemView::EditKeyPressed);
    this->workspaces_->setItemDelegate(
        new WorkspaceDelegate(this->workspaces_));
    connect(this->workspaces_->itemDelegate(),
            &QAbstractItemDelegate::closeEditor, this, [this] {
                this->scheduleRefresh();
            });
    this->workspaces_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->workspaces_->setContextMenuPolicy(Qt::CustomContextMenu);
    nav->addWidget(this->workspaces_, 1);
    this->emptySearch_ = label(tr("No matching chats"), "muted");
    this->emptySearch_->hide();
    nav->addWidget(this->emptySearch_);
    auto *add = button(tr("New chat"));
    add->setObjectName("newChat");
    add->setIcon(QIcon(":/choppa/icons/plus.svg"));
    add->setToolTip(tr("Open an empty chat tab"));
    nav->addWidget(add);
    nav->addSpacing(8);
    auto *settings = button(tr("Settings"));
    settings->setIcon(QIcon(":/choppa/icons/settings.svg"));
    nav->addWidget(settings);
    nav->addSpacing(8);
    this->account_ = button(tr("Sign in to Twitch"), "account");
    this->account_->setIcon(QIcon(":/choppa/icons/account.svg"));
    this->account_->setIconSize(QSize(18, 18));
    this->account_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    nav->addWidget(this->account_);
    body->addWidget(this->sidebar_);

    auto *workspace = new QVBoxLayout;
    workspace->setContentsMargins(0, 0, 0, 0);
    workspace->setSpacing(4);
    auto *heading = new QHBoxLayout;
    auto *toggle = button(QString(), "windowControl");
    toggle->setIcon(QIcon(":/choppa/icons/sidebar.svg"));
    toggle->setFixedSize(28, 28);
    toggle->setToolTip(tr("Toggle sidebar"));
    toggle->setAccessibleName(tr("Toggle sidebar"));
    heading->addWidget(toggle);
    auto *titles = new QVBoxLayout;
    titles->setSpacing(4);
    this->title_ = label(tr("Channels"), "heading");
    this->title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    this->subtitle_ = label(tr("Choose a channel to get started."), "muted");
    titles->addWidget(this->title_);
    titles->addWidget(this->subtitle_);
    this->subtitle_->hide();
    heading->addLayout(titles, 1);
    auto *split = button(tr("+ Split"), "secondary");
    auto *channel = button(tr("+ Channel"), "secondary");
    split->setToolTip(tr("Add a chat pane to this tab"));
    channel->setToolTip(tr("Open a channel in a new tab"));
    heading->addWidget(split);
    heading->addWidget(channel);
    workspace->addLayout(heading);
    this->compactNavigation_ = new QWidget;
    auto *compactLayout = new QVBoxLayout(this->compactNavigation_);
    compactLayout->setContentsMargins(0, 0, 0, 0);
    compactLayout->setSpacing(0);
    workspace->addWidget(this->compactNavigation_);
    this->notebook_->setExternalNavigation(true);
    this->notebook_->installEventFilter(this);
    workspace->addWidget(this->notebook_, 1);
    body->addLayout(workspace, 1);

    connect(add, &QPushButton::clicked, this, [this] {
        this->notebook_->addPage(true);
    });
    connect(channel, &QPushButton::clicked, this, [this] {
        this->addChannel(true);
    });
    connect(split, &QPushButton::clicked, this, [this] {
        this->addChannel(false);
    });
    connect(toggle, &QPushButton::clicked, this, [this] {
        this->sidebarExpanded_ = !this->sidebar_->isVisible();
        this->updateNavigationLayout();
    });
    connect(settings, &QPushButton::clicked, this, [this] {
        getApp()->getWindows()->showSettingsDialog(this->window_);
    });
    connect(this->account_, &QPushButton::clicked, this, [this] {
        if (getApp()->getAccounts()->twitch.getUsernames().empty())
        {
            getApp()->getWindows()->showSettingsDialog(
                this->window_, SettingsDialogPreference::Accounts);
            return;
        }
        getApp()->getWindows()->showAccountSelectPopup(
            this->account_->mapToGlobal(QPoint(0, 0)), this->window_);
    });
    this->signals_.managedConnect(
        getApp()->getAccounts()->twitch.currentUserChanged, [this] {
            this->updateAccount();
        });
    connect(this->filter_, &QLineEdit::textChanged, this, [this] {
        this->refreshNavigation();
    });
    connect(this->filter_, &QLineEdit::returnPressed, this, [this] {
        for (int i = 0; i < this->workspaces_->count(); ++i)
            if (!this->workspaces_->item(i)->isHidden())
            {
                this->notebook_->selectIndex(i);
                break;
            }
    });
    connect(this->workspaces_->model(), &QAbstractItemModel::rowsMoved, this,
            [this](const QModelIndex &, int first, int last,
                   const QModelIndex &, int destination) {
                if (first != last)
                    return;
                const int target =
                    destination > first ? destination - 1 : destination;
                auto *page = this->notebook_->getPageAt(first);
                this->notebook_->rearrangePage(page, target, true);
            });
    connect(this->workspaces_, &QListWidget::currentRowChanged, this,
            [this](int row) {
                auto *item = this->workspaces_->item(row);
                if (!item)
                    return;
                auto *page = item->data(Qt::UserRole).value<QWidget *>();
                if (this->notebook_->indexOf(page) >= 0)
                    this->notebook_->select(page);
            });
    connect(this->workspaces_, &QListWidget::itemChanged, this,
            [this](QListWidgetItem *item) {
                auto *widget = item->data(Qt::UserRole).value<QWidget *>();
                if (this->notebook_->indexOf(widget) < 0)
                    return;
                auto *page = static_cast<SplitContainer *>(widget);
                if (page->getTab()->getTitle() != item->text().trimmed())
                    page->getTab()->setCustomTitle(item->text().trimmed());
            });
    connect(this->workspaces_, &QListWidget::customContextMenuRequested, this,
            &ChoppaShell::showWorkspaceMenu);
    connect(this->notebook_, &Notebook::navigationChanged, this,
            &ChoppaShell::scheduleRefresh);

    this->updateAccount();
    this->scheduleRefresh();
}

void ChoppaShell::focusNavigation()
{
    this->sidebarExpanded_ = true;
    this->updateNavigationLayout();
    this->filter_->setFocus();
    this->filter_->selectAll();
}

void ChoppaShell::scheduleRefresh()
{
    // Coalesce layout/title/live/unread notifications within one event-loop turn.
    // No polling timer and no reconstruction of chat pages on navigation changes.
    if (this->refreshPending_)
        return;
    this->refreshPending_ = true;
    QTimer::singleShot(0, this, [this] {
        this->refreshPending_ = false;
        this->refreshNavigation();
    });
}

void ChoppaShell::refreshNavigation()
{
    // Live/unread updates must not replace text while an inline rename is active.
    if (auto *editor = this->workspaces_->findChild<QLineEdit *>();
        editor && editor->isVisible())
        return;
    const QSignalBlocker blocker(this->workspaces_);
    const bool canReorder = this->filter_->text().trimmed().isEmpty();
    this->workspaces_->setDragEnabled(canReorder);
    this->workspaces_->setAcceptDrops(canReorder);
    const int count = this->notebook_->getPageCount();
    while (this->workspaces_->count() > count)
        delete this->workspaces_->takeItem(this->workspaces_->count() - 1);
    int visible = 0;
    for (int i = 0; i < count; ++i)
    {
        auto *page =
            dynamic_cast<SplitContainer *>(this->notebook_->getPageAt(i));
        if (!page)
            continue;
        auto *tab = page->getTab();
        auto name = tab->getTitle();
        if (name.isEmpty())
            name = tr("New chat");
        auto *item = this->workspaces_->item(i);
        if (!item)
        {
            item = new QListWidgetItem(this->workspaces_);
            item->setFlags(item->flags() | Qt::ItemIsEditable);
        }
        item->setData(Qt::UserRole,
                      QVariant::fromValue(static_cast<QWidget *>(page)));
        item->setText(name);
        QStringList status;
        if (tab->isLive())
            status << tr("LIVE: red dot");
        if (tab->highlightState() == HighlightState::Highlighted)
            status << tr("Mention / highlight: amber underline");
        else if (tab->highlightState() == HighlightState::NewMessage)
            status << tr("Unread messages: bold name");
        item->setToolTip(
            name + (status.isEmpty() ? QString() : "\n" + status.join("\n")));
        item->setData(LiveRole, tab->isLive());
        item->setData(HighlightRole, static_cast<int>(tab->highlightState()));
        item->setData(Qt::AccessibleDescriptionRole, status.join(", "));
        const bool hidden = !this->notebook_->isPageVisible(i) ||
                            !name.contains(this->filter_->text().trimmed(),
                                           Qt::CaseInsensitive);
        item->setHidden(hidden);
        visible += !hidden;
    }
    this->workspaces_->setCurrentRow(this->notebook_->getSelectedIndex());
    this->emptySearch_->setVisible(visible == 0 &&
                                   !this->filter_->text().isEmpty());
    auto *page = this->notebook_->getSelectedPage();
    if (page && !page->getSplits().empty())
    {
        this->title_->setText(page->getTab()->getTitle());
        const auto paneCount = page->getSplits().size();
        this->subtitle_->setText(
            paneCount == 1 ? tr("1 chat view in this tab")
                           : tr("%1 chat views in this tab").arg(paneCount));
    }
    else
    {
        this->title_->setText(tr("Channels"));
        this->subtitle_->setText(tr("Choose a channel to get started."));
    }
    this->updateNavigationLayout();
}

void ChoppaShell::updateAccount()
{
    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    this->account_->setText(account->isAnon() ? tr("Sign in to Twitch")
                                              : account->getUserName());
    this->account_->setToolTip(tr("Manage Twitch accounts"));
}

void ChoppaShell::addChannel(bool newWorkspace)
{
    auto *page = newWorkspace ? this->notebook_->addPage(true)
                              : this->notebook_->getOrAddSelectedPage();
    auto *split = page->appendNewSplit(false);
    const QPointer<SplitNotebook> notebook(this->notebook_);
    const QPointer<SplitContainer> guardedPage(page);
    const QPointer<Split> guardedSplit(split);
    split->showChangeChannelPopup(
        "Open channel", true,
        [notebook, guardedPage, guardedSplit, newWorkspace](bool accepted) {
            if (accepted || !notebook || !guardedPage || !guardedSplit)
                return;
            guardedPage->deleteSplit(guardedSplit);
            if (newWorkspace && guardedPage->getSplits().empty())
            {
                notebook->removePage(guardedPage);
                if (notebook->getPageCount() == 0)
                    notebook->addPage(true);
            }
        });
}

void ChoppaShell::showWorkspaceMenu(const QPoint &position)
{
    auto *item = this->workspaces_->itemAt(position);
    if (!item)
        return;
    const int index = this->workspaces_->row(item);
    auto *page =
        dynamic_cast<SplitContainer *>(this->notebook_->getPageAt(index));
    if (!page)
        return;
    page->getTab()->showContextMenu(
        this->workspaces_->viewport()->mapToGlobal(position));
}

bool ChoppaShell::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == this->notebook_ && event->type() == QEvent::Resize)
    {
        QPainterPath corners;
        corners.addRoundedRect(QRectF(this->notebook_->rect()), 8, 8);
        this->notebook_->setMask(QRegion(corners.toFillPolygon().toPolygon()));
    }
    return QWidget::eventFilter(watched, event);
}

void ChoppaShell::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    this->updateNavigationLayout();
}

void ChoppaShell::updateNavigationLayout()
{
    const bool compact = !this->sidebarExpanded_;
    if (this->workspaces_->property("compact").toBool() != compact ||
        this->workspaces_->parentWidget() !=
            (compact ? this->compactNavigation_ : this->sidebar_))
    {
        this->workspaces_->setProperty("compact", compact);
        this->workspaces_->setViewMode(compact ? QListView::IconMode
                                               : QListView::ListMode);
        this->workspaces_->setMovement(QListView::Snap);
        this->workspaces_->setResizeMode(QListView::Adjust);
        this->workspaces_->setWrapping(compact);
        this->workspaces_->setGridSize(compact ? QSize(84, 28) : QSize());
        if (compact)
            this->compactNavigation_->layout()->addWidget(this->workspaces_);
        else
            static_cast<QVBoxLayout *>(this->sidebar_->layout())
                ->insertWidget(2, this->workspaces_, 1);
        const bool canReorder = this->filter_->text().trimmed().isEmpty();
        this->workspaces_->setDragEnabled(canReorder);
        this->workspaces_->setAcceptDrops(canReorder);
        this->workspaces_->show();
    }
    this->sidebar_->setVisible(!compact);
    this->compactNavigation_->setVisible(compact);
    if (compact)
    {
        const int columns = std::max(1, (this->width() - 18) / 84);
        const int rows = std::clamp(
            (this->workspaces_->count() + columns - 1) / columns, 1, 4);
        this->compactNavigation_->setFixedHeight(rows * 28 + 2);
    }
}
}  // namespace chatterino
