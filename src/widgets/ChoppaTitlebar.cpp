// SPDX-License-Identifier: MIT
#include "widgets/ChoppaTitlebar.hpp"

#include <QHBoxLayout>
#include <QFocusEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QWindow>

namespace chatterino {
namespace {
class WindowControl final : public QAbstractButton
{
public:
    explicit WindowControl(int action)
        : action_(action)
    {
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_Hover);
    }

protected:
    void focusInEvent(QFocusEvent *event) override
    {
        keyboardFocus_ = event->reason() == Qt::TabFocusReason ||
                         event->reason() == Qt::BacktabFocusReason ||
                         event->reason() == Qt::ShortcutFocusReason;
        QAbstractButton::focusInEvent(event);
        update();
    }

    void focusOutEvent(QFocusEvent *event) override
    {
        keyboardFocus_ = false;
        QAbstractButton::focusOutEvent(event);
        update();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(underMouse() || isDown()
                       ? QColor(action_ == 2
                                    ? (isDown() ? "#922334" : "#b32d3b")
                                    : "#242424")
                       : QColor("#0a0a0a"));
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 5, 5);
        if (hasFocus() && keyboardFocus_)
        {
            p.setBrush(Qt::NoBrush);
            p.setPen(QColor("#777777"));
            p.drawRoundedRect(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5), 4, 4);
        }
        p.setPen(QPen(QColor("#eeeeee"), 1.25));
        const QPointF c(width() / 2.0, height() / 2.0);
        if (action_ == 0)
            p.drawLine(c + QPointF(-4, 0), c + QPointF(4, 0));
        else if (action_ == 1)
            p.drawRect(QRectF(c.x() - 3.5, c.y() - 3.5, 7, 7));
        else
        {
            p.drawLine(c + QPointF(-3.5, -3.5), c + QPointF(3.5, 3.5));
            p.drawLine(c + QPointF(-3.5, 3.5), c + QPointF(3.5, -3.5));
        }
    }

private:
    int action_;
    bool keyboardFocus_ = false;
};
}  // namespace

ChoppaTitlebar::ChoppaTitlebar(QWidget *window)
    : QWidget(window)
{
    this->setObjectName("choppaTitlebar");
    this->setProperty("choppaDragRegion", true);
    this->setAttribute(Qt::WA_StyledBackground);
    this->setStyleSheet(R"(
        #choppaTitlebar { background: #0a0a0a; }
        #choppaTitlebar QLabel { background: transparent; border: none; color: #999999; font: 12px 'Outfit'; }
        #choppaTitlebar QLabel#brand { color: white; font: 600 15px 'Outfit'; }
        #choppaTitlebar QPushButton { border: 1px solid transparent; border-radius: 6px; background: transparent; color: #cccccc; padding: 0; font: 18px 'Outfit'; }
        #choppaTitlebar QPushButton:hover { background: #1a1a1a; color: white; }
        #choppaTitlebar QPushButton:focus { border-color: #77777d; }
        #choppaTitlebar QPushButton#close:hover { background: #b32d3b; }
    )");
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 3, 6, 3);
    layout->setSpacing(8);
    auto *logo = new QLabel;
    logo->setPixmap(
        QPixmap(":/choppa/logo.png")
            .scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(logo);
    auto *brand = new QLabel("choppa<span style='color:#77777d'>.lol</span>");
    brand->setObjectName("brand");
    brand->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(brand);
    layout->addSpacing(12);
    auto *title = new QLabel;
    title->setTextFormat(Qt::PlainText);
    title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    title->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(title, 1);
    auto updateTitle = [title](const QString &text) {
        // The brand is already displayed on the left of every titlebar.
        title->setText(text.startsWith("choppa.lol") ? QString() : text);
    };
    connect(window, &QWidget::windowTitleChanged, title, updateTitle);
    updateTitle(window->windowTitle());
    const auto controls = {
        std::pair<QString, QString>{QString::fromUtf8("\xe2\x88\x92"),
                                    tr("Minimize")},
        {QString::fromUtf8("\xe2\x96\xa1"), tr("Maximize / restore")},
        {QString::fromUtf8("\xc3\x97"), tr("Close")}};
    int index = 0;
    for (const auto &[text, name] : controls)
    {
        const int action = index++;
        auto *control = new WindowControl(action);
        control->setFixedSize(30, 24);
        control->setToolTip(name);
        control->setAccessibleName(name);
        control->setObjectName(action == 2 ? "close" : "windowControl");
        control->setCursor(Qt::PointingHandCursor);
        layout->addWidget(control);
        connect(control, &QAbstractButton::clicked, this, [window, action] {
            if (action == 0)
                window->showMinimized();
            else if (action == 1)
                window->isMaximized() ? window->showNormal()
                                      : window->showMaximized();
            else
                window->close();
        });
    }
}

void ChoppaTitlebar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && this->window()->windowHandle())
        this->window()->windowHandle()->startSystemMove();
    else
        QWidget::mousePressEvent(event);
}

void ChoppaTitlebar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        this->window()->isMaximized() ? this->window()->showNormal()
                                      : this->window()->showMaximized();
    else
        QWidget::mouseDoubleClickEvent(event);
}
}  // namespace chatterino
