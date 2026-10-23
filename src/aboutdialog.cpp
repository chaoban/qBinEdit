// qBinEdit - About dialog (SAISAI.Dev branding) and license viewer
#include "aboutdialog.h"
#include "appinfo.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

namespace {

bool isDarkPalette(const QPalette &p)
{
    return p.color(QPalette::Window).lightness() < 128;
}

QString link(const QString &url, const QString &text = QString())
{
    return QStringLiteral("<a href=\"%1\">%2</a>").arg(url.toHtmlEscaped(), (text.isEmpty() ? url : text).toHtmlEscaped());
}

// Rounded banner with the SAISAI logo: beige + charcoal logo in light mode,
// dark brown + white logo in dark mode.
class BrandBanner : public QWidget
{
public:
    explicit BrandBanner(QWidget *parent)
        : QWidget(parent)
    {
        setFixedHeight(124);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setToolTip(QString::fromLatin1(AppInfo::kDeveloper));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const bool dark = isDarkPalette(palette());
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()), 16, 16);
        p.fillPath(path, dark ? QColor(0x38, 0x28, 0x1F) : QColor(0xF5, 0xEC, 0xDC));

        // Pre-scale the high-resolution artwork to the exact device pixel size (sharp on any DPI).
        const qreal dpr = devicePixelRatioF();
        const qreal w = width() * 0.66;
        const QSize phys(qRound(w * dpr), qRound(height() * 0.6 * dpr));
        if (m_cache.isNull() || m_cacheDark != dark || m_cacheSize != phys) {
            const QPixmap logo(dark ? QStringLiteral(":/brand/saisai_logo_dark.png")
                                    : QStringLiteral(":/brand/saisai_logo_light.png"));
            m_cache = logo.isNull() ? QPixmap()
                                    : logo.scaled(phys, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_cache.setDevicePixelRatio(dpr);
            m_cacheDark = dark;
            m_cacheSize = phys;
        }
        if (m_cache.isNull())
            return;
        const QSizeF ls = QSizeF(m_cache.size()) / dpr;
        p.drawPixmap(QPointF((width() - ls.width()) / 2, (height() - ls.height()) / 2), m_cache);
    }

    void changeEvent(QEvent *e) override
    {
        if (e->type() == QEvent::PaletteChange)
            update();
        QWidget::changeEvent(e);
    }

private:
    QPixmap m_cache;
    QSize m_cacheSize;
    bool m_cacheDark = false;
};

QLabel *dimLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    QPalette pal = l->palette();
    QColor c = pal.color(QPalette::WindowText);
    c.setAlpha(170);
    pal.setColor(QPalette::WindowText, c);
    l->setPalette(pal);
    return l;
}

} // namespace

// ============================================================================

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About qBinEdit"));
    setFixedWidth(600);

    auto *banner = new BrandBanner(this);

    auto *icon = new QLabel(this);
    icon->setPixmap(QIcon(QStringLiteral(":/icons/qbinedit.png")).pixmap(QSize(76, 76)));
    icon->setFixedSize(80, 80);
    icon->setAlignment(Qt::AlignCenter);

    auto *name = new QLabel(QStringLiteral("qBinEdit"), this);
    QFont nf = name->font();
    nf.setPointSizeF(nf.pointSizeF() * 1.9);
    nf.setBold(true);
    name->setFont(nf);
    auto *version = dimLabel(tr("Version %1").arg(QStringLiteral(QBE_VERSION)), this);
    auto *subtitle = dimLabel(tr("Firmware binary viewer / editor"), this);

    auto *titleBox = new QVBoxLayout;
    titleBox->setSpacing(2);
    titleBox->addStretch();
    titleBox->addWidget(name);
    titleBox->addWidget(version);
    titleBox->addWidget(subtitle);
    titleBox->addStretch();

    auto *head = new QHBoxLayout;
    head->setSpacing(16);
    head->addWidget(icon);
    head->addLayout(titleBox, 1);

    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);

    const QString dev = QString::fromLatin1(AppInfo::kDeveloper);
    const QString email = QString::fromLatin1(AppInfo::kEmail);
    const QString sourceUrl = QString::fromLatin1(AppInfo::kSourceUrl);
    const QString privacyUrl = QString::fromLatin1(AppInfo::kPrivacyUrl);

    QString html;
    html += QStringLiteral("<p style='margin-top:0'><b style='font-size:large'>© %1 %2</b><br>")
                .arg(QString::fromLatin1(AppInfo::kCopyrightYear), dev.toHtmlEscaped());
    html += tr("qBinEdit is free software released under the MIT License, without any warranty.").toHtmlEscaped();
    if (!sourceUrl.isEmpty())
        html += QStringLiteral("<br>") + tr("Source code: %1").toHtmlEscaped().arg(link(sourceUrl));
    html += QStringLiteral("</p><p>");
    html += tr("Contact: %1").toHtmlEscaped().arg(link(QStringLiteral("mailto:") + email, email));
    html += QStringLiteral("<br>");
    if (!privacyUrl.isEmpty())
        html += tr("Privacy policy: %1").toHtmlEscaped().arg(link(privacyUrl));
    else
        html += tr("Privacy: qBinEdit collects no data and makes no network connections.").toHtmlEscaped();
    html += QStringLiteral("</p><p>");
    html += tr("Built with Qt %1. Qt is used under the GNU LGPL v3.").arg(QString::fromLatin1(qVersion())).toHtmlEscaped();
    html += QStringLiteral("<br>") + tr("Qt source code: %1").toHtmlEscaped().arg(link(QString::fromLatin1(AppInfo::kQtSourceUrl)));
    html += QStringLiteral("<br>")
            + tr("Other third-party components and full license texts: see \"License Information\".").toHtmlEscaped();
    html += QStringLiteral("</p>");

    auto *body = new QLabel(html, this);
    body->setTextFormat(Qt::RichText);
    body->setWordWrap(true);
    body->setOpenExternalLinks(true);
    body->setTextInteractionFlags(Qt::TextBrowserInteraction);

    auto *btnLicense = new QPushButton(tr("License Information..."), this);
    auto *btnQt = new QPushButton(tr("About Qt..."), this);
    auto *btnClose = new QPushButton(tr("Close"), this);
    btnClose->setDefault(true);
    for (QPushButton *b : { btnLicense, btnQt, btnClose })
        b->setMinimumWidth(130);
    connect(btnLicense, &QPushButton::clicked, this, [this]() {
        LicenseDialog dlg(this);
        dlg.exec();
    });
    connect(btnQt, &QPushButton::clicked, this, []() { QApplication::aboutQt(); });
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(btnLicense);
    buttons->addWidget(btnQt);
    buttons->addWidget(btnClose);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(20, 20, 20, 20);
    lay->setSpacing(14);
    lay->addWidget(banner);
    lay->addLayout(head);
    lay->addWidget(line);
    lay->addWidget(body);
    lay->addSpacing(4);
    lay->addLayout(buttons);
    adjustSize();
}

// ============================================================================

LicenseDialog::LicenseDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("License Information"));
    resize(900, 620);

    struct Entry { QString title; QString path; };
    const QList<Entry> entries = {
        { tr("qBinEdit license"), QStringLiteral(":/licenses/LICENSE.txt") },
        { tr("Third-party notices"), QStringLiteral(":/licenses/THIRD-PARTY-NOTICES.txt") },
        { QStringLiteral("GNU LGPL v3 (Qt)"), QStringLiteral(":/licenses/LGPL-3.0.txt") },
        { QStringLiteral("GNU GPL v3"), QStringLiteral(":/licenses/GPL-3.0.txt") },
        { QStringLiteral("GCC Runtime Library Exception 3.1"), QStringLiteral(":/licenses/GCC-Runtime-Library-Exception-3.1.txt") },
        { QStringLiteral("MinGW-w64 winpthreads"), QStringLiteral(":/licenses/winpthreads.txt") },
        { QStringLiteral("zlib"), QStringLiteral(":/licenses/zlib.txt") },
        { QStringLiteral("libpng"), QStringLiteral(":/licenses/libpng.txt") },
        { QStringLiteral("PCRE2"), QStringLiteral(":/licenses/PCRE2-BSD-3-Clause.txt") },
        { QStringLiteral("MIT"), QStringLiteral(":/licenses/MIT.txt") },
        { QStringLiteral("BSD-3-Clause"), QStringLiteral(":/licenses/BSD-3-Clause.txt") },
    };

    auto *list = new QListWidget(this);
    for (const Entry &e : entries) {
        auto *it = new QListWidgetItem(e.title, list);
        it->setData(Qt::UserRole, e.path);
    }
    list->setMinimumWidth(230);

    auto *text = new QPlainTextEdit(this);
    text->setReadOnly(true);
    text->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    auto *split = new QSplitter(this);
    split->addWidget(list);
    split->addWidget(text);
    split->setStretchFactor(1, 1);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(split, 1);
    lay->addWidget(bb);

    connect(list, &QListWidget::currentItemChanged, this, [text](QListWidgetItem *it) {
        if (!it)
            return;
        QFile f(it->data(Qt::UserRole).toString());
        text->setPlainText(f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString());
        text->moveCursor(QTextCursor::Start);
    });
    list->setCurrentRow(0);
}
