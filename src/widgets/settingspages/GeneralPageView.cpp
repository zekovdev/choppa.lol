// SPDX-FileCopyrightText: 2020 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/GeneralPageView.hpp"

#include "Application.hpp"
#include "common/QLogging.hpp"
#include "util/LayoutHelper.hpp"
#include "util/RapidJsonSerializeQString.hpp"
#include "widgets/helper/Line.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>

namespace {

constexpr int MAX_TOOLTIP_LINE_LENGTH = 50;
const auto MAX_TOOLTIP_LINE_LENGTH_PATTERN =
    QStringLiteral(R"(.{%1}\S*\K(\s+))").arg(MAX_TOOLTIP_LINE_LENGTH);
const QRegularExpression MAX_TOOLTIP_LINE_LENGTH_REGEX(
    MAX_TOOLTIP_LINE_LENGTH_PATTERN);
}  // namespace

namespace chatterino {

GeneralPageView::GeneralPageView(QWidget *parent)
    : QWidget(parent)
    , contentScrollArea_(new QScrollArea)
    , contentLayout_(new QVBoxLayout)
{
    auto *contentWidget = new QWidget;
    contentWidget->setLayout(this->contentLayout_);
    this->contentScrollArea_->setWidget(contentWidget);
    this->contentScrollArea_->setObjectName("generalSettingsScrollContent");
    this->contentScrollArea_->setWidgetResizable(true);
}

GeneralPageView *GeneralPageView::withoutNavigation(QWidget *parent)
{
    auto *view = new GeneralPageView(parent);

    view->setLayout(makeLayout<QHBoxLayout>({
        view->contentScrollArea_,
    }));

    return view;
}

GeneralPageView *GeneralPageView::withNavigation(QWidget *parent)
{
    auto *view = new GeneralPageView(parent);

    auto *layout = new QVBoxLayout(view);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *header = new QHBoxLayout;
    header->addWidget(new QLabel(tr("Jump to section")));
    view->sectionPicker_ = new QComboBox;
    view->sectionPicker_->setAccessibleName(tr("Settings section"));
    header->addWidget(view->sectionPicker_, 1);
    layout->addLayout(header);
    layout->addWidget(view->contentScrollArea_, 1);
    QObject::connect(
        view->sectionPicker_, &QComboBox::activated, view, [view](int index) {
            if (index >= 0 && index < static_cast<int>(view->groups_.size()))
                view->contentScrollArea_->verticalScrollBar()->setValue(
                    view->groups_[index].title->y());
        });

    QObject::connect(view->contentScrollArea_->verticalScrollBar(),
                     &QScrollBar::valueChanged, view, [view] {
                         view->updateNavigationHighlighting();
                     });

    return view;
}

void GeneralPageView::addWidget(QWidget *widget, const QStringList &keywords)
{
    this->contentLayout_->addWidget(widget);
    if (!this->groups_.empty())
    {
        this->groups_.back().widgets.push_back({
            .element = widget,
            .keywords = keywords,
        });
    }
}

void GeneralPageView::registerWidget(QWidget *widget,
                                     const QStringList &keywords,
                                     QWidget *parentElement)
{
    if (!this->groups_.empty())
    {
        this->groups_.back().widgets.push_back({
            .element = widget,
            .keywords = keywords,
            .parentElement = parentElement,
        });
    }
}

void GeneralPageView::pushWidget(QWidget *widget)
{
    this->contentLayout_->addWidget(widget);
}

void GeneralPageView::addLayout(QLayout *layout)
{
    this->contentLayout_->addLayout(layout);
}

void GeneralPageView::addStretch()
{
    this->contentLayout_->addStretch();
}

TitleLabel *GeneralPageView::addTitle(const QString &title)
{
    // space
    if (!this->groups_.empty())
    {
        this->addWidget(this->groups_.back().space = new Space);
    }

    // title
    auto *label = new TitleLabel(title);
    this->addWidget(label);

    NavigationLabel *navLabel = nullptr;

    // navigation item
    if (this->navigationLayout_ != nullptr)
    {
        navLabel = new NavigationLabel(title);
        navLabel->setCursor(Qt::PointingHandCursor);
        this->navigationLayout_->addWidget(navLabel);

        QObject::connect(
            navLabel, &NavigationLabel::leftMouseUp, label, [this, label] {
                this->contentScrollArea_->verticalScrollBar()->setValue(
                    label->y());
            });
    }

    // groups
    this->groups_.push_back(Group{title, label, navLabel, nullptr, {}});
    if (this->sectionPicker_)
        this->sectionPicker_->addItem(title);

    if (this->groups_.size() == 1)
    {
        this->updateNavigationHighlighting();
    }

    return label;
}

SubtitleLabel *GeneralPageView::addSubtitle(const QString &title)
{
    auto *label = new SubtitleLabel(title + ":");
    this->addWidget(label);

    this->groups_.back().widgets.push_back({label, {title}});

    return label;
}

ComboBox *GeneralPageView::addDropdown(const QString &text,
                                       const QStringList &list,
                                       QString toolTipText)
{
    auto *layout = new QHBoxLayout;
    auto *combo = new ComboBox;
    combo->setFocusPolicy(Qt::StrongFocus);
    combo->addItems(list);

    auto *label = new QLabel(text + ":");
    layout->addWidget(label);
    layout->addStretch(1);
    layout->addWidget(combo);

    this->addToolTip(*label, toolTipText);
    this->addLayout(layout);

    // groups
    this->groups_.back().widgets.push_back({combo, {text}});
    this->groups_.back().widgets.push_back({label, {text}});

    return combo;
}

void GeneralPageView::addNavigationSpacing()
{
    if (this->navigationLayout_)
        this->navigationLayout_->addSpacing(24);
}

DescriptionLabel *GeneralPageView::addDescription(const QString &text)
{
    auto *label = new DescriptionLabel(text);

    label->setTextInteractionFlags(Qt::TextBrowserInteraction |
                                   Qt::LinksAccessibleByKeyboard);
    label->setOpenExternalLinks(true);
    label->setWordWrap(true);

    this->addWidget(label);

    // groups
    this->groups_.back().widgets.push_back({label, {text}});

    return label;
}

void GeneralPageView::addSeparator()
{
    this->addWidget(new Line(false));
}

bool GeneralPageView::filterElements(const QString &query)
{
    if (this->sectionPicker_)
        this->sectionPicker_->setEnabled(query.isEmpty());
    bool any{};

    for (auto &&group : this->groups_)
    {
        // if a description in a group matches `query` then show the entire group
        bool descriptionMatches{};
        for (auto &&widget : group.widgets)
        {
            if (auto *x = dynamic_cast<DescriptionLabel *>(widget.element); x)
            {
                if (x->text().contains(query, Qt::CaseInsensitive))
                {
                    descriptionMatches = true;
                    break;
                }
            }
        }

        // if group name matches then all should be visible
        if (group.name.contains(query, Qt::CaseInsensitive) ||
            descriptionMatches)
        {
            for (auto &&widget : group.widgets)
            {
                widget.element->show();
                if (widget.parentElement != nullptr)
                {
                    widget.parentElement->show();
                }
            }

            group.title->show();
            if (group.navigationLink != nullptr)
            {
                group.navigationLink->show();
            }
            any = true;
        }
        // check if any match
        else
        {
            auto groupAny = false;

            QWidget *currentSubtitle = nullptr;
            bool currentSubtitleVisible = false;
            bool currentSubtitleSearched = false;

            for (auto &&widget : group.widgets)
            {
                if (auto *x = dynamic_cast<SubtitleLabel *>(widget.element))
                {
                    currentSubtitleSearched = false;
                    if (currentSubtitle)
                    {
                        currentSubtitle->setVisible(currentSubtitleVisible);
                    }

                    currentSubtitleVisible = false;
                    currentSubtitle = widget.element;

                    if (x->text().contains(query, Qt::CaseInsensitive))
                    {
                        currentSubtitleSearched = true;
                    }
                    continue;
                }

                if (auto *x = dynamic_cast<Line *>(widget.element))
                {
                    // Hide lines in search when not searching for full categories
                    x->hide();
                }

                for (auto &&keyword : widget.keywords)
                {
                    if (keyword.contains(query, Qt::CaseInsensitive) ||
                        currentSubtitleSearched)
                    {
                        currentSubtitleVisible = true;
                        widget.element->show();
                        if (widget.parentElement != nullptr)
                        {
                            widget.parentElement->show();
                        }
                        groupAny = true;
                        break;
                    }

                    widget.element->hide();
                    if (widget.parentElement != nullptr)
                    {
                        widget.parentElement->hide();
                    }
                }
            }

            if (currentSubtitle)
            {
                currentSubtitle->setVisible(currentSubtitleVisible);
            }

            if (group.space)
            {
                group.space->setVisible(groupAny);
            }

            group.title->setVisible(groupAny);
            if (group.navigationLink != nullptr)
            {
                group.navigationLink->setVisible(groupAny);
            }
            any |= groupAny;
        }
    }

    return any;
}

void GeneralPageView::updateNavigationHighlighting()
{
    if (this->sectionPicker_ && !this->groups_.empty())
    {
        const int scrollY =
            this->contentScrollArea_->verticalScrollBar()->value();
        int current = 0;
        for (int i = 0; i < static_cast<int>(this->groups_.size()); ++i)
            if (this->groups_[i].title->y() <= scrollY + 24)
                current = i;
        this->sectionPicker_->setCurrentIndex(current);
    }
    if (this->navigationLayout_ == nullptr)
    {
        return;
    }

    auto scrollY = this->contentScrollArea_->verticalScrollBar()->value();
    auto first = true;

    for (auto &&group : this->groups_)
    {
        if (first && (group.title->geometry().bottom() > scrollY ||
                      &group == &this->groups_.back()))
        {
            first = false;
            group.navigationLink->setStyleSheet("color: #d6d6d6");
        }
        else
        {
            group.navigationLink->setStyleSheet("");
        }
    }
}

void GeneralPageView::addToolTip(QWidget &widget, QString text) const
{
    if (text.isEmpty())
    {
        return;
    }

    if (text.length() > MAX_TOOLTIP_LINE_LENGTH)
    {
        // match MAX_TOOLTIP_LINE_LENGTH characters, any remaining
        // non-space, and then capture the following space for
        // replacement with newline
        text.replace(MAX_TOOLTIP_LINE_LENGTH_REGEX, "\n");
    }

    widget.setToolTip(text);
}

}  // namespace chatterino
