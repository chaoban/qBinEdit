// qBinEdit - main window
#include "mainwindow.h"
#include "aboutdialog.h"
#include "appsettings.h"
#include "checksumdialog.h"
#include "checksums.h"
#include "datainspector.h"
#include "dialogs.h"
#include "document.h"
#include "finddialog.h"
#include "hexview.h"
#include "tabpages.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QUndoGroup>
#include <QUndoStack>
#include <climits>

namespace {

constexpr int kMaxRecent = 10;
constexpr qint64 kMaxClipboard = qint64(32) * 1024 * 1024;

QIcon themeIcon(const char *name, QStyle::StandardPixmap fallback)
{
    QIcon ic = QIcon::fromTheme(QString::fromLatin1(name));
    if (ic.isNull())
        ic = qApp->style()->standardIcon(fallback);
    return ic;
}

QIcon textIcon(const QString &text)
{
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::TextAntialiasing);
    QFont f = qApp->font();
    f.setBold(true);
    f.setPixelSize(text.size() > 3 ? 18 : (text.size() > 2 ? 22 : 30));
    p.setFont(f);
    p.setPen(qApp->palette().color(QPalette::WindowText));
    p.drawText(pm.rect(), Qt::AlignCenter, text);
    return QIcon(pm);
}

QString hexStr(quint64 v, int digits = 1)
{
    return QString::number(v, 16).toUpper().rightJustified(digits, QLatin1Char('0'));
}

QString fileFilters()
{
    return QStringList{
        MainWindow::tr("All files (*)"),
        MainWindow::tr("Firmware files (*.bin *.hex *.ihx *.s19 *.s28 *.s37 *.srec *.mot *.rom *.img *.fw *.dfu)"),
        MainWindow::tr("Binary image (*.bin *.rom *.img *.fw)"),
        MainWindow::tr("Intel HEX (*.hex *.ihx)"),
        MainWindow::tr("Motorola S-Record (*.s19 *.s28 *.s37 *.srec *.mot)"),
    }.join(QStringLiteral(";;"));
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setAcceptDrops(true);
    m_undoGroup = new QUndoGroup(this);

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setElideMode(Qt::ElideMiddle);

    m_welcome = new QLabel(this);
    m_welcome->setAlignment(Qt::AlignCenter);
    m_welcome->setWordWrap(true);
    m_welcome->setText(tr("<h2>qBinEdit</h2>"
                          "<p>Open a firmware file with <b>Ctrl+O</b>, or drag and drop it here.</p>"
                          "<p>Raw binary, Intel HEX (.hex) and Motorola S-Record (.s19 / .srec / .mot) are supported.<br>"
                          "Switch between binary, octal and hexadecimal display from the toolbar or the View menu.</p>"));

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_welcome);
    m_stack->addWidget(m_tabs);
    setCentralWidget(m_stack);

    loadSettings();
    createActions();
    createDock();
    createMenus();
    createToolBar();
    createStatusBar();

    connect(m_tabs, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int i) { closeTab(i); });

    QSettings &s = AppSettings::settings();
    if (!restoreGeometry(s.value(QStringLiteral("window/geometry")).toByteArray()))
        resize(1180, 760);
    if (!restoreState(s.value(QStringLiteral("window/state")).toByteArray()))
        resizeDocks({ m_inspectorDock }, { 340 }, Qt::Horizontal);

    syncViewControls();
    updateActions();
    updateStatus();
    updateTitle();
}

// ============================================================================
// construction

void MainWindow::createActions()
{
    auto mk = [this](const QString &text, const QKeySequence &ks, auto slot) {
        auto *a = new QAction(text, this);
        if (!ks.isEmpty())
            a->setShortcut(ks);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };

    m_actNew = mk(tr("&New"), QKeySequence::New, [this]() { newFile(); });
    m_actNew->setIcon(themeIcon("document-new", QStyle::SP_FileIcon));
    m_actOpen = mk(tr("&Open..."), QKeySequence::Open, [this]() { openDialog(); });
    m_actOpen->setIcon(themeIcon("document-open", QStyle::SP_DialogOpenButton));
    m_actOpenRaw = mk(tr("Open as &Raw Binary..."), QKeySequence(), [this]() { openRawDialog(); });
    m_actSave = mk(tr("&Save"), QKeySequence::Save, [this]() { save(); });
    m_actSave->setIcon(themeIcon("document-save", QStyle::SP_DialogSaveButton));
    m_actSaveAs = mk(tr("Save &As / Export..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), [this]() { saveAs(); });
    m_actClose = mk(tr("&Close Tab"), QKeySequence(Qt::CTRL | Qt::Key_W), [this]() { closeTab(m_tabs->currentIndex()); });
    m_actProps = mk(tr("File &Properties..."), QKeySequence(Qt::ALT | Qt::Key_Return), [this]() { showProperties(); });
    m_actExit = mk(tr("E&xit"), QKeySequence(), [this]() { close(); });

    m_actUndo = m_undoGroup->createUndoAction(this, tr("&Undo"));
    m_actUndo->setShortcut(QKeySequence::Undo);
    m_actUndo->setIcon(themeIcon("edit-undo", QStyle::SP_ArrowBack));
    m_actRedo = m_undoGroup->createRedoAction(this, tr("&Redo"));
    m_actRedo->setShortcuts({ QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z) });
    m_actRedo->setIcon(themeIcon("edit-redo", QStyle::SP_ArrowForward));

    m_actCopy = mk(tr("&Copy"), QKeySequence::Copy, [this]() { copyAs(-1); });
    m_actCopy->setIcon(themeIcon("edit-copy", QStyle::SP_FileDialogDetailedView));
    m_actCopyHex = mk(tr("Hex Text (DE AD BE EF)"), QKeySequence(), [this]() { copyAs(0); });
    m_actCopyHexCompact = mk(tr("Hex String (DEADBEEF)"), QKeySequence(), [this]() { copyAs(1); });
    m_actCopyBin = mk(tr("Binary Text (11011110 ...)"), QKeySequence(), [this]() { copyAs(2); });
    m_actCopyC = mk(tr("C Array"), QKeySequence(), [this]() { copyAs(3); });
    m_actCopyText = mk(tr("ASCII Text"), QKeySequence(), [this]() { copyAs(4); });
    m_actPaste = mk(tr("&Paste (Overwrite)"), QKeySequence::Paste, [this]() { paste(false); });
    m_actPaste->setIcon(themeIcon("edit-paste", QStyle::SP_DialogApplyButton));
    m_actPasteInsert = mk(tr("Paste &Insert"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V), [this]() { paste(true); });
    m_actSelectAll = mk(tr("Select &All"), QKeySequence::SelectAll, [this]() {
        if (HexView *v = currentView())
            v->selectAll();
    });
    m_actSelectRange = mk(tr("Select &Range..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R), [this]() { selectRange(); });
    m_actFill = mk(tr("&Fill Selection..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L), [this]() { fillSelection(); });
    m_actInsert = mk(tr("&Insert Bytes..."), QKeySequence(Qt::Key_Insert), [this]() { insertBytes(); });
    m_actDelete = mk(tr("&Delete Selection"), QKeySequence::Delete, [this]() { deleteSelection(); });
    m_actEditMode = new QAction(tr("&Edit Mode"), this);
    m_actEditMode->setCheckable(true);
    m_actEditMode->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    m_actEditMode->setToolTip(tr("Edit mode (Ctrl+E). Files open read-only to protect firmware images."));
    m_actEditMode->setIcon(themeIcon("system-lock-screen", QStyle::SP_DialogResetButton));
    connect(m_actEditMode, &QAction::triggered, this, &MainWindow::toggleEditMode);

    m_actFind = mk(tr("&Find..."), QKeySequence::Find, [this]() { showFind(false); });
    m_actFind->setIcon(themeIcon("edit-find", QStyle::SP_FileDialogContentsView));
    m_actReplace = mk(tr("&Replace..."), QKeySequence(Qt::CTRL | Qt::Key_H), [this]() { showFind(true); });
    m_actFindNext = mk(tr("Find &Next"), QKeySequence(Qt::Key_F3), [this]() { findNext(false); });
    m_actFindPrev = mk(tr("Find Pre&vious"), QKeySequence(Qt::SHIFT | Qt::Key_F3), [this]() { findNext(true); });
    m_actGoto = mk(tr("&Go To Offset / Address..."), QKeySequence(Qt::CTRL | Qt::Key_G), [this]() { gotoOffset(); });
    m_actGoto->setIcon(themeIcon("go-next", QStyle::SP_ArrowRight));
    m_actBase = mk(tr("Set &Base Address..."), QKeySequence(), [this]() { setBaseAddress(); });

    m_actChecksum = mk(tr("&Checksum / CRC..."), QKeySequence(Qt::CTRL | Qt::Key_K), [this]() { showChecksum(); });
    m_actChecksum->setIcon(textIcon(QStringLiteral("CRC")));
    m_actCompare = mk(tr("Co&mpare With File..."), QKeySequence(Qt::CTRL | Qt::Key_D), [this]() { compareWith(); });
    m_actCompare->setIcon(textIcon(QStringLiteral("A≠B")));
    m_actNextDiff = mk(tr("&Next Difference"), QKeySequence(Qt::Key_F8), [this]() { nextDifference(false); });
    m_actPrevDiff = mk(tr("&Previous Difference"), QKeySequence(Qt::SHIFT | Qt::Key_F8), [this]() { nextDifference(true); });

    // --- view: radix ---
    m_radixGroup = new QActionGroup(this);
    const struct { int radix; const char *text; QKeySequence ks; } radixes[] = {
        { 2, QT_TR_NOOP("&Binary (Base 2)"), QKeySequence(Qt::CTRL | Qt::Key_1) },
        { 8, QT_TR_NOOP("&Octal (Base 8)"), QKeySequence(Qt::CTRL | Qt::Key_2) },
        { 16, QT_TR_NOOP("&Hexadecimal (Base 16)"), QKeySequence(Qt::CTRL | Qt::Key_3) },
    };
    for (const auto &r : radixes) {
        QAction *a = m_radixGroup->addAction(tr(r.text));
        a->setCheckable(true);
        a->setData(r.radix);
        a->setShortcut(r.ks);
    }
    connect(m_radixGroup, &QActionGroup::triggered, this, [this](QAction *a) {
        if (!m_syncing)
            setRadix(a->data().toInt());
    });

    m_unitGroup = new QActionGroup(this);
    const struct { int bytes; const char *text; QKeySequence ks; } units[] = {
        { 1, QT_TR_NOOP("&8-bit (Byte)"), QKeySequence(Qt::ALT | Qt::Key_1) },
        { 2, QT_TR_NOOP("&16-bit (Word)"), QKeySequence(Qt::ALT | Qt::Key_2) },
        { 4, QT_TR_NOOP("&32-bit (DWord)"), QKeySequence(Qt::ALT | Qt::Key_3) },
    };
    for (const auto &u : units) {
        QAction *a = m_unitGroup->addAction(tr(u.text));
        a->setCheckable(true);
        a->setData(u.bytes);
        a->setShortcut(u.ks);
    }
    connect(m_unitGroup, &QActionGroup::triggered, this, [this](QAction *a) {
        if (!m_syncing)
            setUnit(a->data().toInt());
    });

    m_endianGroup = new QActionGroup(this);
    for (int big = 0; big < 2; ++big) {
        QAction *a = m_endianGroup->addAction(big ? tr("&Big Endian") : tr("&Little Endian"));
        a->setCheckable(true);
        a->setData(big);
    }
    connect(m_endianGroup, &QActionGroup::triggered, this, [this](QAction *a) {
        if (!m_syncing)
            setBigEndian(a->data().toInt() != 0);
    });

    m_bprGroup = new QActionGroup(this);
    for (int n : { 8, 16, 32 }) {
        QAction *a = m_bprGroup->addAction(tr("%1 Bytes per Row").arg(n));
        a->setCheckable(true);
        a->setData(n);
    }
    connect(m_bprGroup, &QActionGroup::triggered, this, [this](QAction *a) {
        if (!m_syncing)
            setBytesPerRow(a->data().toInt());
    });

    m_actAscii = new QAction(tr("Show &ASCII Column"), this);
    m_actAscii->setCheckable(true);
    connect(m_actAscii, &QAction::triggered, this, &MainWindow::setShowAscii);
    m_actDim = new QAction(tr("&Dim 00 / FF Values"), this);
    m_actDim->setCheckable(true);
    connect(m_actDim, &QAction::triggered, this, &MainWindow::setDimFill);
    m_actZoomIn = mk(tr("Zoom &In"), QKeySequence::ZoomIn, [this]() { zoom(1); });
    m_actZoomOut = mk(tr("Zoom &Out"), QKeySequence::ZoomOut, [this]() { zoom(-1); });
    m_actZoomReset = mk(tr("&Reset Zoom"), QKeySequence(Qt::CTRL | Qt::Key_0), [this]() { zoom(0); });
    m_actFont = mk(tr("&Font..."), QKeySequence(), [this]() { chooseFont(); });

    m_langGroup = new QActionGroup(this);
    const QString lang = AppSettings::settings().value(QStringLiteral("ui/language"), QStringLiteral("auto")).toString();
    const struct { const char *code; QString text; } langs[] = {
        { "auto", tr("Follow &System") },
        { "en", QStringLiteral("English") },
        { "zh_TW", QStringLiteral("繁體中文") },
    };
    for (const auto &l : langs) {
        QAction *a = m_langGroup->addAction(l.text);
        a->setCheckable(true);
        a->setData(QString::fromLatin1(l.code));
        a->setChecked(lang == QLatin1String(l.code));
    }
    connect(m_langGroup, &QActionGroup::triggered, this, [this](QAction *a) { setLanguage(a->data().toString()); });

    m_actAbout = mk(tr("&About qBinEdit"), QKeySequence(), [this]() { about(); });
    m_actAboutQt = mk(tr("About &Qt"), QKeySequence(), []() { QApplication::aboutQt(); });
}

void MainWindow::createDock()
{
    m_inspector = new DataInspector(this);
    m_inspectorDock = new QDockWidget(tr("Data Inspector"), this);
    m_inspectorDock->setObjectName(QStringLiteral("inspectorDock"));
    m_inspectorDock->setWidget(m_inspector);
    addDockWidget(Qt::RightDockWidgetArea, m_inspectorDock);
    m_actInspector = m_inspectorDock->toggleViewAction();
    m_actInspector->setText(tr("Data &Inspector"));
    m_actInspector->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
}

void MainWindow::createMenus()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->addAction(m_actNew);
    file->addAction(m_actOpen);
    file->addAction(m_actOpenRaw);
    m_recentMenu = file->addMenu(tr("Recent &Files"));
    updateRecentMenu();
    file->addSeparator();
    file->addAction(m_actSave);
    file->addAction(m_actSaveAs);
    file->addSeparator();
    file->addAction(m_actProps);
    file->addAction(m_actClose);
    file->addSeparator();
    file->addAction(m_actExit);

    QMenu *edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(m_actUndo);
    edit->addAction(m_actRedo);
    edit->addSeparator();
    edit->addAction(m_actCopy);
    m_copyAsMenu = edit->addMenu(tr("Copy &As"));
    m_copyAsMenu->addAction(m_actCopyHex);
    m_copyAsMenu->addAction(m_actCopyHexCompact);
    m_copyAsMenu->addAction(m_actCopyBin);
    m_copyAsMenu->addAction(m_actCopyC);
    m_copyAsMenu->addAction(m_actCopyText);
    edit->addAction(m_actPaste);
    edit->addAction(m_actPasteInsert);
    edit->addSeparator();
    edit->addAction(m_actSelectAll);
    edit->addAction(m_actSelectRange);
    edit->addSeparator();
    edit->addAction(m_actFill);
    edit->addAction(m_actInsert);
    edit->addAction(m_actDelete);
    edit->addSeparator();
    edit->addAction(m_actEditMode);

    QMenu *search = menuBar()->addMenu(tr("&Search"));
    search->addAction(m_actFind);
    search->addAction(m_actReplace);
    search->addAction(m_actFindNext);
    search->addAction(m_actFindPrev);
    search->addSeparator();
    search->addAction(m_actGoto);

    QMenu *view = menuBar()->addMenu(tr("&View"));
    QMenu *radix = view->addMenu(tr("&Display Base"));
    radix->addActions(m_radixGroup->actions());
    QMenu *unit = view->addMenu(tr("Data &Width"));
    unit->addActions(m_unitGroup->actions());
    QMenu *endian = view->addMenu(tr("Byte &Order"));
    endian->addActions(m_endianGroup->actions());
    QMenu *bpr = view->addMenu(tr("Bytes per &Row"));
    bpr->addActions(m_bprGroup->actions());
    view->addSeparator();
    view->addAction(m_actAscii);
    view->addAction(m_actDim);
    view->addSeparator();
    view->addAction(m_actZoomIn);
    view->addAction(m_actZoomOut);
    view->addAction(m_actZoomReset);
    view->addAction(m_actFont);
    view->addSeparator();
    view->addAction(m_actInspector);
    QMenu *lang = view->addMenu(tr("&Language"));
    lang->addActions(m_langGroup->actions());

    QMenu *tools = menuBar()->addMenu(tr("&Tools"));
    tools->addAction(m_actChecksum);
    tools->addSeparator();
    tools->addAction(m_actCompare);
    tools->addAction(m_actNextDiff);
    tools->addAction(m_actPrevDiff);
    tools->addSeparator();
    tools->addAction(m_actBase);

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(m_actAbout);
    help->addAction(m_actAboutQt);
}

void MainWindow::createToolBar()
{
    QToolBar *tb = addToolBar(tr("Main Toolbar"));
    tb->setObjectName(QStringLiteral("mainToolBar"));
    tb->setIconSize(QSize(20, 20));
    tb->addAction(m_actNew);
    tb->addAction(m_actOpen);
    tb->addAction(m_actSave);
    tb->addSeparator();
    tb->addAction(m_actUndo);
    tb->addAction(m_actRedo);
    tb->addSeparator();
    tb->addAction(m_actEditMode);
    if (auto *b = qobject_cast<QToolButton *>(tb->widgetForAction(m_actEditMode)))
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    tb->addSeparator();

    auto addCombo = [this, tb](const QString &label, QComboBox *combo) {
        auto *l = new QLabel(label, tb);
        l->setContentsMargins(6, 0, 3, 0);
        tb->addWidget(l);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        tb->addWidget(combo);
        Q_UNUSED(this);
    };

    m_radixCombo = new QComboBox(tb);
    m_radixCombo->addItem(tr("Binary (2)"), 2);
    m_radixCombo->addItem(tr("Octal (8)"), 8);
    m_radixCombo->addItem(tr("Hex (16)"), 16);
    m_radixCombo->setToolTip(tr("Display base (Ctrl+1 / Ctrl+2 / Ctrl+3)"));
    addCombo(tr("Display:"), m_radixCombo);

    m_unitCombo = new QComboBox(tb);
    m_unitCombo->addItem(tr("8-bit"), 1);
    m_unitCombo->addItem(tr("16-bit"), 2);
    m_unitCombo->addItem(tr("32-bit"), 4);
    m_unitCombo->setToolTip(tr("Data width (Alt+1 / Alt+2 / Alt+3)"));
    addCombo(tr("Width:"), m_unitCombo);

    m_endianCombo = new QComboBox(tb);
    m_endianCombo->addItem(tr("Little endian"), 0);
    m_endianCombo->addItem(tr("Big endian"), 1);
    addCombo(tr("Order:"), m_endianCombo);

    m_bprCombo = new QComboBox(tb);
    for (int n : { 8, 16, 32 })
        m_bprCombo->addItem(QString::number(n), n);
    addCombo(tr("Bytes/row:"), m_bprCombo);

    connect(m_radixCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        if (!m_syncing)
            setRadix(m_radixCombo->currentData().toInt());
    });
    connect(m_unitCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        if (!m_syncing)
            setUnit(m_unitCombo->currentData().toInt());
    });
    connect(m_endianCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        if (!m_syncing)
            setBigEndian(m_endianCombo->currentData().toInt() != 0);
    });
    connect(m_bprCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        if (!m_syncing)
            setBytesPerRow(m_bprCombo->currentData().toInt());
    });

    tb->addSeparator();
    tb->addAction(m_actFind);
    tb->addAction(m_actGoto);
    tb->addAction(m_actChecksum);
    tb->addAction(m_actCompare);
}

void MainWindow::createStatusBar()
{
    auto mkLabel = [this]() {
        auto *l = new QLabel(this);
        l->setContentsMargins(6, 0, 6, 0);
        return l;
    };
    m_stPos = mkLabel();
    m_stValue = mkLabel();
    m_stSel = mkLabel();
    m_stSize = mkLabel();
    m_stFormat = mkLabel();
    m_stMode = new QToolButton(this);
    m_stMode->setAutoRaise(true);
    m_stMode->setToolTip(tr("Click or press Ctrl+E to toggle edit mode"));
    connect(m_stMode, &QToolButton::clicked, m_actEditMode, &QAction::trigger);

    statusBar()->addPermanentWidget(m_stPos);
    statusBar()->addPermanentWidget(m_stValue);
    statusBar()->addPermanentWidget(m_stSel);
    statusBar()->addPermanentWidget(m_stSize);
    statusBar()->addPermanentWidget(m_stFormat);
    statusBar()->addPermanentWidget(m_stMode);
}

void MainWindow::loadSettings()
{
    QSettings &s = AppSettings::settings();
    m_defRadix = s.value(QStringLiteral("view/radix"), 16).toInt();
    m_defUnit = s.value(QStringLiteral("view/unit"), 1).toInt();
    m_defBig = s.value(QStringLiteral("view/bigEndian"), false).toBool();
    m_defBpr = s.value(QStringLiteral("view/bytesPerRow"), 16).toInt();
    m_defAscii = s.value(QStringLiteral("view/ascii"), true).toBool();
    m_defDim = s.value(QStringLiteral("view/dimFill"), true).toBool();
    m_defFont = s.value(QStringLiteral("view/font")).toString();
    m_lastDir = s.value(QStringLiteral("lastDir"), QDir::homePath()).toString();
    if (m_defRadix != 2 && m_defRadix != 8 && m_defRadix != 16)
        m_defRadix = 16;
    if (m_defUnit != 1 && m_defUnit != 2 && m_defUnit != 4)
        m_defUnit = 1;
    if (m_defBpr != 8 && m_defBpr != 16 && m_defBpr != 32)
        m_defBpr = 16;
}

void MainWindow::saveSettings()
{
    QSettings &s = AppSettings::settings();
    s.setValue(QStringLiteral("view/radix"), m_defRadix);
    s.setValue(QStringLiteral("view/unit"), m_defUnit);
    s.setValue(QStringLiteral("view/bigEndian"), m_defBig);
    s.setValue(QStringLiteral("view/bytesPerRow"), m_defBpr);
    s.setValue(QStringLiteral("view/ascii"), m_defAscii);
    s.setValue(QStringLiteral("view/dimFill"), m_defDim);
    s.setValue(QStringLiteral("view/font"), m_defFont);
    s.setValue(QStringLiteral("lastDir"), m_lastDir);
    s.setValue(QStringLiteral("window/geometry"), saveGeometry());
    s.setValue(QStringLiteral("window/state"), saveState());
    s.sync();
}

void MainWindow::setInitialViewOptions(int radix, int unitBytes, int bigEndian)
{
    if (radix == 2 || radix == 8 || radix == 16) {
        if (radix == 2 && m_defRadix != 2 && m_defBpr > 8) {
            m_bprBeforeBinary = m_defBpr;   // binary rows are wide: start with 8 bytes per row
            m_defBpr = 8;
        }
        m_defRadix = radix;
    }
    if (unitBytes == 1 || unitBytes == 2 || unitBytes == 4)
        m_defUnit = unitBytes;
    if (bigEndian >= 0)
        m_defBig = bigEndian != 0;
    syncViewControls();
}

// ============================================================================
// tabs

TabPage *MainWindow::currentPage() const
{
    return qobject_cast<TabPage *>(m_tabs->currentWidget());
}

EditorPage *MainWindow::currentEditor() const
{
    return qobject_cast<EditorPage *>(m_tabs->currentWidget());
}

HexView *MainWindow::currentView() const
{
    TabPage *p = currentPage();
    return p ? p->currentView() : nullptr;
}

Document *MainWindow::currentDocument() const
{
    HexView *v = currentView();
    return v ? v->document() : nullptr;
}

void MainWindow::applyDefaults(HexView *v)
{
    v->setUnitSize(m_defUnit);
    v->setBytesPerRow(m_defBpr);
    v->setRadix(m_defRadix);
    v->setBigEndian(m_defBig);
    v->setShowAscii(m_defAscii);
    v->setDimFillBytes(m_defDim);
    if (!m_defFont.isEmpty()) {
        QFont f;
        if (f.fromString(m_defFont))
            v->setViewFont(f);
    }
}

void MainWindow::wireView(HexView *v)
{
    connect(v, &HexView::cursorPositionChanged, this, [this, v]() {
        if (v == currentView())
            updateStatus();
    });
    connect(v, &HexView::selectionChanged, this, [this, v]() {
        if (v == currentView()) {
            updateStatus();
            updateActions();
        }
    });
    connect(v, &HexView::statusMessage, this, &MainWindow::statusMessage);
    connect(v, &HexView::contextMenuRequested, this, &MainWindow::showContextMenu);
}

void MainWindow::addPage(TabPage *page)
{
    const int idx = m_tabs->addTab(page, page->title());
    m_tabs->setTabToolTip(idx, page->toolTip());
    connect(page, &TabPage::titleChanged, this, [this, page]() {
        const int i = m_tabs->indexOf(page);
        if (i >= 0) {
            m_tabs->setTabText(i, page->title());
            m_tabs->setTabToolTip(i, page->toolTip());
        }
        updateTitle();
        updateActions();
    });
    connect(page, &TabPage::activeViewChanged, this, [this, page]() {
        if (page == currentPage()) {
            syncViewControls();
            updateStatus();
            updateActions();
        }
    });
    for (HexView *v : page->views()) {
        applyDefaults(v);
        wireView(v);
    }
    if (Document *d = page->document()) {
        m_undoGroup->addStack(d->undoStack());
        connect(d, &Document::readOnlyChanged, this, [this]() {
            updateActions();
            updateStatus();
        });
        connect(d, &Document::contentsChanged, this, [this, d]() {
            if (d == currentDocument())
                updateStatus();
        });
    }
    m_tabs->setCurrentIndex(idx);
    m_stack->setCurrentWidget(m_tabs);
    onCurrentTabChanged();
    page->currentView()->setFocus();
}

void MainWindow::onCurrentTabChanged()
{
    TabPage *p = currentPage();
    if (!p) {
        m_stack->setCurrentWidget(m_welcome);
        m_undoGroup->setActiveStack(nullptr);
    } else {
        m_stack->setCurrentWidget(m_tabs);
        Document *d = p->document();
        m_undoGroup->setActiveStack(d ? d->undoStack() : nullptr);
    }
    syncViewControls();
    updateActions();
    updateStatus();
    updateTitle();
}

bool MainWindow::closeTab(int index)
{
    auto *page = qobject_cast<TabPage *>(m_tabs->widget(index));
    if (!page)
        return false;
    if (auto *ep = qobject_cast<EditorPage *>(page)) {
        if (!maybeSave(ep))
            return false;
        m_undoGroup->removeStack(ep->document()->undoStack());
    }
    m_tabs->removeTab(m_tabs->indexOf(page));
    page->deleteLater();
    onCurrentTabChanged();
    return true;
}

bool MainWindow::maybeSave(EditorPage *page)
{
    Document *d = page->document();
    if (!d->isModified())
        return true;
    m_tabs->setCurrentWidget(page);
    const auto ret = QMessageBox::warning(this, tr("Unsaved Changes"),
                                          tr("\"%1\" has been modified.\nDo you want to save your changes?").arg(d->displayName()),
                                          QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (ret == QMessageBox::Save)
        return saveDocument(d, false);
    return ret == QMessageBox::Discard;
}

// ============================================================================
// file

void MainWindow::newFile()
{
    auto *d = new Document;
    d->setContent(QByteArray(), 0, tr("Untitled %1").arg(++m_untitled));
    d->setReadOnly(false);
    addPage(new EditorPage(d));
}

void MainWindow::openDialog()
{
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Open Firmware File"), m_lastDir, fileFilters());
    for (const QString &f : files)
        openFile(f);
}

void MainWindow::openRawDialog()
{
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Open as Raw Binary"), m_lastDir, tr("All files (*)"));
    for (const QString &f : files)
        openFile(f, int(FileFormat::Binary));
}

bool MainWindow::openFile(const QString &path, int forceFormat)
{
    const QString abs = QFileInfo(path).absoluteFilePath();
    if (forceFormat < 0) {
        for (int i = 0; i < m_tabs->count(); ++i) {
            auto *ep = qobject_cast<EditorPage *>(m_tabs->widget(i));
            if (ep && ep->document()->filePath() == abs) {
                m_tabs->setCurrentIndex(i);
                return true;
            }
        }
    }

    auto *doc = new Document;
    QString err;
    QStringList warnings;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = doc->load(abs, forceFormat, &err, &warnings);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        delete doc;
        bool textFormat = false;
        if (forceFormat < 0) {
            QFile f(abs);
            if (f.open(QIODevice::ReadOnly))
                textFormat = Formats::detect(abs, f.read(1024)) != FileFormat::Binary;
        }
        QMessageBox box(QMessageBox::Warning, tr("Cannot Open File"), err, QMessageBox::Close, this);
        QPushButton *raw = textFormat ? box.addButton(tr("Open as Raw Binary"), QMessageBox::AcceptRole) : nullptr;
        box.exec();
        if (raw && box.clickedButton() == raw)
            return openFile(abs, int(FileFormat::Binary));
        return false;
    }

    m_lastDir = QFileInfo(abs).absolutePath();
    addPage(new EditorPage(doc));
    addRecent(abs);
    statusMessage(tr("Opened %1 (%2, %3 bytes)")
                      .arg(QDir::toNativeSeparators(abs), Formats::formatName(doc->format()), QLocale().toString(doc->size())));
    if (!warnings.isEmpty()) {
        QStringList shown = warnings.mid(0, 25);
        if (warnings.size() > 25)
            shown << tr("... and %1 more").arg(warnings.size() - 25);
        QMessageBox::warning(this, tr("Warnings while loading %1").arg(doc->displayName()), shown.join(QLatin1Char('\n')));
    }
    return true;
}

bool MainWindow::save()
{
    EditorPage *ep = currentEditor();
    return ep && saveDocument(ep->document(), false);
}

bool MainWindow::saveAs()
{
    EditorPage *ep = currentEditor();
    return ep && saveDocument(ep->document(), true);
}

bool MainWindow::saveDocument(Document *doc, bool forceDialog)
{
    QString path = doc->filePath();
    FileFormat fmt = doc->format();
    if (forceDialog || path.isEmpty()) {
        const QString binF = tr("Binary image (*.bin *.rom *.img *.fw)");
        const QString hexF = tr("Intel HEX (*.hex *.ihx)");
        const QString srecF = tr("Motorola S-Record (*.s19 *.s28 *.s37 *.srec *.mot)");
        const QString allF = tr("All files (*)");
        QString selected = fmt == FileFormat::IntelHex ? hexF : (fmt == FileFormat::SRecord ? srecF : binF);
        const QString start = path.isEmpty() ? QDir(m_lastDir).filePath(doc->displayName()) : path;
        path = QFileDialog::getSaveFileName(this, tr("Save As / Export"), start,
                                            QStringList{ binF, hexF, srecF, allF }.join(QStringLiteral(";;")), &selected);
        if (path.isEmpty())
            return false;
        const QString ext = QFileInfo(path).suffix().toLower();
        if (selected == hexF)
            fmt = FileFormat::IntelHex;
        else if (selected == srecF)
            fmt = FileFormat::SRecord;
        else if (selected == binF)
            fmt = FileFormat::Binary;
        else if (ext == QLatin1String("hex") || ext == QLatin1String("ihx"))
            fmt = FileFormat::IntelHex;
        else if (QStringList{ QStringLiteral("s19"), QStringLiteral("s28"), QStringLiteral("s37"),
                              QStringLiteral("srec"), QStringLiteral("mot") }.contains(ext))
            fmt = FileFormat::SRecord;
        else
            fmt = FileFormat::Binary;
        m_lastDir = QFileInfo(path).absolutePath();

        if (fmt != FileFormat::Binary && doc->format() == FileFormat::Binary) {
            bool ok = false;
            const QString t = QInputDialog::getText(this, tr("Start Address"),
                                                    tr("Load address of the data in the %1 file (hex):").arg(Formats::formatName(fmt)),
                                                    QLineEdit::Normal, hexStr(doc->baseAddress(), 8), &ok);
            if (!ok)
                return false;
            quint64 base = 0;
            if (!parseNumber(t, true, &base)) {
                QMessageBox::warning(this, tr("Start Address"), tr("Invalid address."));
                return false;
            }
            doc->setBaseAddress(base);
        }
    }
    QString err;
    if (!doc->save(path, fmt, &err)) {
        QMessageBox::critical(this, tr("Save Failed"), err);
        return false;
    }
    addRecent(path);
    statusMessage(tr("Saved %1 (%2)").arg(QDir::toNativeSeparators(path), Formats::formatName(fmt)));
    updateTitle();
    updateStatus();
    return true;
}

void MainWindow::showProperties()
{
    Document *d = currentDocument();
    if (!d)
        return;
    const auto &fi = d->formatInfo();
    QStringList lines;
    lines << tr("File: %1").arg(d->filePath().isEmpty() ? d->displayName() : QDir::toNativeSeparators(d->filePath()));
    lines << tr("Format: %1").arg(Formats::formatName(d->format()));
    lines << tr("Size: %1 bytes (0x%2)").arg(QLocale().toString(d->size()), hexStr(quint64(d->size())));
    lines << tr("Address range: 0x%1 - 0x%2")
                 .arg(hexStr(d->baseAddress(), 8), hexStr(d->baseAddress() + quint64(qMax<qint64>(1, d->size())) - 1, 8));
    if (d->format() != FileFormat::Binary) {
        lines << tr("Records: %1").arg(fi.recordCount);
        lines << tr("Segments in file: %1").arg(fi.chunks.size());
        int n = 0;
        for (const auto &c : fi.chunks) {
            if (++n > 16) {
                lines << QStringLiteral("    ...");
                break;
            }
            lines << QStringLiteral("    0x%1 - 0x%2  (%3 bytes)")
                         .arg(hexStr(c.address, 8), hexStr(c.address + quint64(c.data.size()) - 1, 8))
                         .arg(c.data.size());
        }
        if (fi.hasStart)
            lines << tr("Start address: 0x%1").arg(hexStr(fi.startAddress, 8));
        if (!fi.srecHeader.isEmpty())
            lines << tr("S0 header: %1").arg(QString::fromLatin1(fi.srecHeader).trimmed());
    }
    for (const auto &spec : Checksums::crcCatalog()) {
        if (QByteArray(spec.name).startsWith("CRC-32/ISO-HDLC")) {
            const quint32 crc = Checksums::crcCompute(spec, reinterpret_cast<const uchar *>(d->data().constData()), d->size());
            lines << tr("CRC-32: 0x%1").arg(hexStr(crc, 8));
        }
    }
    lines << tr("State: %1%2").arg(d->isReadOnly() ? tr("read-only") : tr("editable"),
                                   d->isModified() ? tr(", modified") : QString());
    QMessageBox box(QMessageBox::Information, tr("File Properties"), lines.join(QLatin1Char('\n')), QMessageBox::Ok, this);
    box.setTextInteractionFlags(Qt::TextSelectableByMouse);
    box.exec();
}

void MainWindow::addRecent(const QString &path)
{
    QSettings &s = AppSettings::settings();
    QStringList list = s.value(QStringLiteral("recent")).toStringList();
    list.removeAll(path);
    list.prepend(path);
    while (list.size() > kMaxRecent)
        list.removeLast();
    s.setValue(QStringLiteral("recent"), list);
    updateRecentMenu();
}

void MainWindow::updateRecentMenu()
{
    if (!m_recentMenu)
        return;
    m_recentMenu->clear();
    const QStringList list = AppSettings::settings().value(QStringLiteral("recent")).toStringList();
    for (int i = 0; i < list.size(); ++i) {
        const QString f = list.at(i);
        QAction *a = m_recentMenu->addAction(QStringLiteral("&%1  %2").arg((i + 1) % 10).arg(QDir::toNativeSeparators(f)));
        connect(a, &QAction::triggered, this, [this, f]() { openFile(f); });
    }
    if (list.isEmpty()) {
        m_recentMenu->addAction(tr("(empty)"))->setEnabled(false);
    } else {
        m_recentMenu->addSeparator();
        m_recentMenu->addAction(tr("Clear List"), this, [this]() {
            AppSettings::settings().remove(QStringLiteral("recent"));
            updateRecentMenu();
        });
    }
}

// ============================================================================
// edit

void MainWindow::toggleEditMode(bool editable)
{
    EditorPage *ep = currentEditor();
    if (!ep) {
        m_actEditMode->setChecked(false);
        return;
    }
    ep->document()->setReadOnly(!editable);
    statusMessage(editable ? tr("Edit mode: changes stay in memory until you save (Ctrl+S).")
                           : tr("Read-only mode."));
    updateActions();
    updateStatus();
}

void MainWindow::copyAs(int kind)
{
    HexView *v = currentView();
    if (!v || !v->hasSelection()) {
        statusMessage(tr("Nothing selected."));
        return;
    }
    if (v->selectionLength() > kMaxClipboard) {
        QMessageBox::warning(this, tr("Copy"), tr("The selection is too large to copy as text (limit 32 MiB)."));
        return;
    }
    const QByteArray b = v->selectedBytes();
    if (kind < 0)
        kind = v->asciiPaneActive() ? 4 : 0;
    QString text;
    switch (kind) {
    case 0: text = toHexText(b, 16); break;
    case 1: text = QString::fromLatin1(b.toHex()).toUpper(); break;
    case 2: text = toBinaryText(b); break;
    case 3: text = toCArray(b); break;
    default:
        text.reserve(b.size());
        for (char c : b)
            text += (uchar(c) >= 0x20 && uchar(c) < 0x7F) ? QChar(uchar(c)) : QChar(u'.');
        break;
    }
    QApplication::clipboard()->setText(text);
    statusMessage(tr("Copied %1 bytes.").arg(QLocale().toString(b.size())));
}

void MainWindow::paste(bool insert)
{
    HexView *v = currentView();
    EditorPage *ep = currentEditor();
    if (!v || !ep)
        return;
    Document *d = ep->document();
    if (d->isReadOnly()) {
        statusMessage(tr("Read-only. Press Ctrl+E to enable editing."));
        return;
    }
    const QString text = QApplication::clipboard()->text();
    QByteArray bytes;
    if (v->asciiPaneActive()) {
        bytes = text.toLatin1();
    } else if (!parseHexBytes(text, &bytes)) {
        statusMessage(tr("The clipboard does not contain hex bytes (e.g. \"DE AD BE EF\")."));
        QApplication::beep();
        return;
    }
    if (bytes.isEmpty())
        return;
    const qint64 pos = v->hasSelection() ? v->selectionStart() : v->cursorPosition();
    if (insert)
        d->replace(pos, 0, bytes, tr("Paste Insert"));
    else
        d->replace(pos, qMin<qint64>(bytes.size(), d->size() - pos), bytes, tr("Paste"));
    v->setSelection(pos, bytes.size());
    statusMessage(tr("Pasted %1 bytes at offset 0x%2.").arg(bytes.size()).arg(hexStr(quint64(pos))));
}

void MainWindow::deleteSelection()
{
    HexView *v = currentView();
    EditorPage *ep = currentEditor();
    if (!v || !ep || !v->hasSelection())
        return;
    if (ep->document()->isReadOnly()) {
        statusMessage(tr("Read-only. Press Ctrl+E to enable editing."));
        return;
    }
    const qint64 s = v->selectionStart();
    const qint64 n = v->selectionLength();
    ep->document()->replace(s, n, QByteArray(), tr("Delete"));
    v->setCursorPosition(s);
    statusMessage(tr("Deleted %1 bytes (file size changed).").arg(QLocale().toString(n)));
}

static QByteArray repeatPattern(const QByteArray &pattern, qint64 count)
{
    QByteArray out;
    out.reserve(count);
    while (out.size() < count)
        out += pattern;
    out.truncate(count);
    return out;
}

void MainWindow::insertBytes()
{
    HexView *v = currentView();
    EditorPage *ep = currentEditor();
    if (!v || !ep)
        return;
    if (ep->document()->isReadOnly()) {
        statusMessage(tr("Read-only. Press Ctrl+E to enable editing."));
        return;
    }
    BytesDialog dlg(BytesDialog::Insert, 16, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const qint64 pos = v->cursorPosition();
    const QByteArray data = repeatPattern(dlg.pattern(), dlg.count());
    ep->document()->replace(pos, 0, data, tr("Insert Bytes"));
    v->setSelection(pos, data.size());
}

void MainWindow::fillSelection()
{
    HexView *v = currentView();
    EditorPage *ep = currentEditor();
    if (!v || !ep || !v->hasSelection())
        return;
    if (ep->document()->isReadOnly()) {
        statusMessage(tr("Read-only. Press Ctrl+E to enable editing."));
        return;
    }
    const qint64 s = v->selectionStart();
    const qint64 n = v->selectionLength();
    BytesDialog dlg(BytesDialog::Fill, n, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    ep->document()->replace(s, n, repeatPattern(dlg.pattern(), n), tr("Fill"));
    v->setSelection(s, n);
}

void MainWindow::selectRange()
{
    HexView *v = currentView();
    if (!v || !v->document() || v->document()->size() == 0)
        return;
    const qint64 size = v->document()->size();
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Select Range"));
    auto *from = new QLineEdit(hexStr(quint64(v->hasSelection() ? v->selectionStart() : v->cursorPosition())), &dlg);
    auto *to = new QLineEdit(&dlg);
    to->setPlaceholderText(tr("end offset (inclusive) or +length"));
    auto *form = new QFormLayout(&dlg);
    form->addRow(tr("Start offset (hex):"), from);
    form->addRow(tr("End offset (hex):"), to);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted)
        return;
    quint64 a = 0, b = 0;
    QString t = to->text().trimmed();
    const bool rel = t.startsWith(QLatin1Char('+'));
    if (rel)
        t = t.mid(1);
    if (!parseNumber(from->text(), true, &a) || !parseNumber(t, true, &b)) {
        statusMessage(tr("Invalid range."));
        return;
    }
    if (rel)
        b = a + b - 1;
    if (b < a || qint64(a) >= size) {
        statusMessage(tr("Invalid range."));
        return;
    }
    b = qMin<quint64>(b, quint64(size - 1));
    v->setSelection(qint64(a), qint64(b - a + 1));
}

// ============================================================================
// search

void MainWindow::showFind(bool replace)
{
    if (!m_find) {
        m_find = new FindDialog(this);
        connect(m_find, &FindDialog::findNext, this, [this]() { findNext(false); });
        connect(m_find, &FindDialog::findPrevious, this, [this]() { findNext(true); });
        connect(m_find, &FindDialog::countAll, this, &MainWindow::countMatches);
        connect(m_find, &FindDialog::replaceOne, this, &MainWindow::replaceOne);
        connect(m_find, &FindDialog::replaceAll, this, &MainWindow::replaceAll);
    }
    m_find->setReplaceVisible(replace);
    if (HexView *v = currentView()) {
        if (v->hasSelection() && v->selectionLength() <= 64) {
            const QByteArray b = v->selectedBytes();
            if (v->asciiPaneActive())
                m_find->setFindText(QString::fromLatin1(b), FindDialog::AsciiText);
            else
                m_find->setFindText(toHexText(b, 0), FindDialog::HexBytes);
        }
    }
    m_find->show();
    m_find->raise();
    m_find->activateWindow();
}

void MainWindow::findNext(bool backward)
{
    HexView *v = currentView();
    if (!v || !v->document())
        return;
    if (!m_find) {
        showFind(false);
        return;
    }
    QByteArray pat;
    QString err;
    if (!m_find->findPattern(&pat, &err)) {
        m_find->showResult(err, true);
        if (!m_find->isVisible())
            showFind(false);
        return;
    }
    const bool cs = m_find->caseSensitive();
    const QByteArray hay = cs ? v->document()->data() : v->document()->data().toLower();
    if (!cs)
        pat = pat.toLower();

    qint64 pos = -1;
    bool wrapped = false;
    if (!backward) {
        const qint64 from = v->hasSelection() ? v->selectionStart() + 1 : v->cursorPosition();
        pos = hay.indexOf(pat, from);
        if (pos < 0 && from > 0) {
            pos = hay.indexOf(pat, 0);
            wrapped = pos >= 0;
        }
    } else {
        const qint64 from = (v->hasSelection() ? v->selectionStart() : v->cursorPosition()) - 1;
        pos = from >= 0 ? hay.lastIndexOf(pat, from) : -1;
        if (pos < 0) {
            pos = hay.lastIndexOf(pat);
            wrapped = pos >= 0;
        }
    }
    if (pos < 0) {
        m_find->showResult(tr("Not found."), true);
        statusMessage(tr("Not found."));
        QApplication::beep();
        return;
    }
    v->setSelection(pos, pat.size());
    QString msg = tr("Found at offset 0x%1 (address 0x%2).")
                      .arg(hexStr(quint64(pos)), hexStr(v->document()->baseAddress() + quint64(pos)));
    if (wrapped)
        msg += QLatin1Char(' ') + tr("Search wrapped around.");
    m_find->showResult(msg);
    statusMessage(msg);
}

void MainWindow::countMatches()
{
    HexView *v = currentView();
    if (!v || !v->document() || !m_find)
        return;
    QByteArray pat;
    QString err;
    if (!m_find->findPattern(&pat, &err)) {
        m_find->showResult(err, true);
        return;
    }
    const bool cs = m_find->caseSensitive();
    const QByteArray hay = cs ? v->document()->data() : v->document()->data().toLower();
    if (!cs)
        pat = pat.toLower();
    qint64 n = 0;
    for (qint64 p = hay.indexOf(pat, 0); p >= 0; p = hay.indexOf(pat, p + pat.size()))
        ++n;
    m_find->showResult(tr("%n match(es).", nullptr, int(qMin<qint64>(n, INT_MAX))));
}

void MainWindow::replaceOne()
{
    HexView *v = currentView();
    EditorPage *ep = currentEditor();
    if (!v || !ep || !m_find)
        return;
    if (ep->document()->isReadOnly()) {
        m_find->showResult(tr("Read-only. Press Ctrl+E to enable editing."), true);
        return;
    }
    QByteArray pat, repl;
    QString err;
    if (!m_find->findPattern(&pat, &err) || !m_find->replacePattern(&repl, &err)) {
        m_find->showResult(err, true);
        return;
    }
    const bool cs = m_find->caseSensitive();
    const QByteArray sel = v->selectedBytes();
    const bool matches = v->hasSelection() && sel.size() == pat.size()
                         && (cs ? sel == pat : sel.toLower() == pat.toLower());
    if (matches) {
        const qint64 s = v->selectionStart();
        ep->document()->replace(s, pat.size(), repl, tr("Replace"));
        v->setCursorPosition(s + repl.size());
    }
    findNext(false);
}

void MainWindow::replaceAll()
{
    HexView *v = currentView();
    EditorPage *ep = currentEditor();
    if (!v || !ep || !m_find)
        return;
    Document *d = ep->document();
    if (d->isReadOnly()) {
        m_find->showResult(tr("Read-only. Press Ctrl+E to enable editing."), true);
        return;
    }
    QByteArray pat, repl;
    QString err;
    if (!m_find->findPattern(&pat, &err) || !m_find->replacePattern(&repl, &err)) {
        m_find->showResult(err, true);
        return;
    }
    const bool cs = m_find->caseSensitive();
    const QByteArray hay = cs ? d->data() : d->data().toLower();
    const QByteArray needle = cs ? pat : pat.toLower();
    QVector<qint64> hits;
    for (qint64 p = hay.indexOf(needle, 0); p >= 0; p = hay.indexOf(needle, p + needle.size()))
        hits.append(p);
    if (hits.isEmpty()) {
        m_find->showResult(tr("Not found."), true);
        return;
    }
    if (repl.size() != pat.size()) {
        const auto ret = QMessageBox::question(this, tr("Replace All"),
                                               tr("The replacement has a different length, so the file size will change by %1 bytes. Continue?")
                                                   .arg((repl.size() - pat.size()) * hits.size()));
        if (ret != QMessageBox::Yes)
            return;
    }
    d->undoStack()->beginMacro(tr("Replace All"));
    for (int i = hits.size() - 1; i >= 0; --i)
        d->replace(hits[i], pat.size(), repl, QString());
    d->undoStack()->endMacro();
    m_find->showResult(tr("Replaced %n occurrence(s).", nullptr, int(hits.size())));
}

void MainWindow::gotoOffset()
{
    HexView *v = currentView();
    if (!v || !v->document())
        return;
    Document *d = v->document();
    GotoDialog dlg(d->baseAddress(), d->size(), v->cursorPosition(), this);
    if (dlg.exec() == QDialog::Accepted && dlg.offset() >= 0) {
        v->setCursorPosition(dlg.offset());
        v->setFocus();
    }
}

void MainWindow::setBaseAddress()
{
    EditorPage *ep = currentEditor();
    if (!ep)
        return;
    Document *d = ep->document();
    bool ok = false;
    const QString t = QInputDialog::getText(this, tr("Set Base Address"),
                                            tr("Address of the first byte (hex). Affects the address column and HEX/S-Record export:"),
                                            QLineEdit::Normal, hexStr(d->baseAddress(), 8), &ok);
    if (!ok)
        return;
    quint64 base = 0;
    if (!parseNumber(t, true, &base)) {
        QMessageBox::warning(this, tr("Set Base Address"), tr("Invalid address."));
        return;
    }
    d->setBaseAddress(base);
}

// ============================================================================
// tools

void MainWindow::showChecksum()
{
    HexView *v = currentView();
    if (!v || !v->document())
        return;
    ChecksumDialog dlg(v->document(), v->hasSelection() ? v->selectionStart() : 0, v->selectionLength(), this);
    dlg.exec();
}

void MainWindow::compareWith()
{
    Document *left = nullptr;
    QString err;
    if (EditorPage *ep = currentEditor()) {
        Document *src = ep->document();
        left = new Document;
        left->setContent(src->data(), src->baseAddress(), src->displayName());
    } else {
        const QString p1 = QFileDialog::getOpenFileName(this, tr("Compare: Choose the First File"), m_lastDir, fileFilters());
        if (p1.isEmpty())
            return;
        left = new Document;
        if (!left->load(p1, -1, &err, nullptr)) {
            delete left;
            QMessageBox::warning(this, tr("Compare"), err);
            return;
        }
        m_lastDir = QFileInfo(p1).absolutePath();
    }
    const QString p2 = QFileDialog::getOpenFileName(this, tr("Compare \"%1\" With").arg(left->displayName()), m_lastDir, fileFilters());
    if (p2.isEmpty()) {
        delete left;
        return;
    }
    auto *right = new Document;
    if (!right->load(p2, -1, &err, nullptr)) {
        delete left;
        delete right;
        QMessageBox::warning(this, tr("Compare"), err);
        return;
    }
    m_lastDir = QFileInfo(p2).absolutePath();
    auto *page = new ComparePage(left, right);
    addPage(page);
    statusMessage(tr("%1 differing byte(s).").arg(QLocale().toString(page->differenceCount())));
}

bool MainWindow::compareFiles(const QString &leftPath, const QString &rightPath)
{
    QString err;
    auto *left = new Document;
    auto *right = new Document;
    if (!left->load(leftPath, -1, &err, nullptr) || !right->load(rightPath, -1, &err, nullptr)) {
        delete left;
        delete right;
        QMessageBox::warning(this, tr("Compare"), err);
        return false;
    }
    auto *page = new ComparePage(left, right);
    addPage(page);
    statusMessage(tr("%1 differing byte(s).").arg(QLocale().toString(page->differenceCount())));
    return true;
}

void MainWindow::nextDifference(bool backward)
{
    auto *cp = qobject_cast<ComparePage *>(currentPage());
    if (!cp) {
        statusMessage(tr("Open a comparison first (Tools > Compare With File)."));
        return;
    }
    if (backward)
        cp->previousDifference();
    else
        cp->nextDifference();
}

// ============================================================================
// view

void MainWindow::setRadix(int radix)
{
    const int previous = currentView() ? currentView()->radix() : m_defRadix;
    m_defRadix = radix;
    if (TabPage *p = currentPage())
        for (HexView *v : p->views())
            v->setRadix(radix);
    // Binary rows are 8x wider than hex: switch to 8 bytes/row, and restore afterwards.
    const int bpr = currentView() ? currentView()->bytesPerRow() : m_defBpr;
    if (radix == 2 && previous != 2 && bpr > 8) {
        m_bprBeforeBinary = bpr;
        setBytesPerRow(8);
    } else if (radix != 2 && previous == 2 && m_bprBeforeBinary > 0 && bpr == 8) {
        setBytesPerRow(m_bprBeforeBinary);
        m_bprBeforeBinary = 0;
    }
    syncViewControls();
    updateStatus();
}

void MainWindow::setUnit(int bytes)
{
    m_defUnit = bytes;
    if (TabPage *p = currentPage())
        for (HexView *v : p->views())
            v->setUnitSize(bytes);
    syncViewControls();
    updateStatus();
}

void MainWindow::setBigEndian(bool big)
{
    m_defBig = big;
    if (TabPage *p = currentPage())
        for (HexView *v : p->views())
            v->setBigEndian(big);
    syncViewControls();
    updateStatus();
}

void MainWindow::setBytesPerRow(int n)
{
    m_defBpr = n;
    if (TabPage *p = currentPage())
        for (HexView *v : p->views())
            v->setBytesPerRow(n);
    syncViewControls();
}

void MainWindow::setShowAscii(bool on)
{
    m_defAscii = on;
    if (TabPage *p = currentPage())
        for (HexView *v : p->views())
            v->setShowAscii(on);
}

void MainWindow::setDimFill(bool on)
{
    m_defDim = on;
    if (TabPage *p = currentPage())
        for (HexView *v : p->views())
            v->setDimFillBytes(on);
}

void MainWindow::zoom(int steps)
{
    if (TabPage *p = currentPage())
        for (HexView *v : p->views())
            v->zoom(steps);
}

void MainWindow::chooseFont()
{
    HexView *cv = currentView();
    QFont initial;
    if (cv)
        initial = cv->viewFont();
    else if (!m_defFont.isEmpty())
        initial.fromString(m_defFont);
    bool ok = false;
    const QFont f = QFontDialog::getFont(&ok, initial, this, tr("Select Font"), QFontDialog::MonospacedFonts);
    if (!ok)
        return;
    m_defFont = f.toString();
    for (int i = 0; i < m_tabs->count(); ++i)
        if (auto *p = qobject_cast<TabPage *>(m_tabs->widget(i)))
            for (HexView *v : p->views())
                v->setViewFont(f);
}

void MainWindow::setLanguage(const QString &code)
{
    AppSettings::settings().setValue(QStringLiteral("ui/language"), code);
    QMessageBox::information(this, QStringLiteral("qBinEdit"),
                             QStringLiteral("語言設定會在重新啟動 qBinEdit 後生效。\n"
                                            "The language change takes effect after restarting qBinEdit."));
}

void MainWindow::syncViewControls()
{
    m_syncing = true;
    HexView *v = currentView();
    const int radix = v ? v->radix() : m_defRadix;
    const int unit = v ? v->unitSize() : m_defUnit;
    const bool big = v ? v->bigEndian() : m_defBig;
    const int bpr = v ? v->bytesPerRow() : m_defBpr;
    if (m_radixCombo) {
        m_radixCombo->setCurrentIndex(m_radixCombo->findData(radix));
        m_unitCombo->setCurrentIndex(m_unitCombo->findData(unit));
        m_endianCombo->setCurrentIndex(m_endianCombo->findData(big ? 1 : 0));
        m_bprCombo->setCurrentIndex(m_bprCombo->findData(bpr));
    }
    for (QAction *a : m_radixGroup->actions())
        a->setChecked(a->data().toInt() == radix);
    for (QAction *a : m_unitGroup->actions())
        a->setChecked(a->data().toInt() == unit);
    for (QAction *a : m_endianGroup->actions())
        a->setChecked((a->data().toInt() != 0) == big);
    for (QAction *a : m_bprGroup->actions())
        a->setChecked(a->data().toInt() == bpr);
    m_actAscii->setChecked(v ? v->showAscii() : m_defAscii);
    m_actDim->setChecked(v ? v->dimFillBytes() : m_defDim);
    m_syncing = false;
}

// ============================================================================
// misc

void MainWindow::about()
{
    AboutDialog dlg(this);
    dlg.exec();
}

void MainWindow::showContextMenu(const QPoint &globalPos)
{
    updateActions();
    QMenu menu(this);
    menu.addAction(m_actCopy);
    menu.addMenu(m_copyAsMenu);
    menu.addAction(m_actPaste);
    menu.addAction(m_actPasteInsert);
    menu.addSeparator();
    menu.addAction(m_actSelectAll);
    menu.addAction(m_actSelectRange);
    menu.addSeparator();
    menu.addAction(m_actFill);
    menu.addAction(m_actInsert);
    menu.addAction(m_actDelete);
    menu.addSeparator();
    menu.addAction(m_actChecksum);
    menu.addAction(m_actGoto);
    menu.exec(globalPos);
}

void MainWindow::updateActions()
{
    TabPage *p = currentPage();
    HexView *v = currentView();
    EditorPage *ep = currentEditor();
    const bool has = v && v->document();
    const bool editable = ep && !ep->document()->isReadOnly();
    const bool sel = v && v->hasSelection();

    m_actSave->setEnabled(ep);
    m_actSaveAs->setEnabled(ep);
    m_actClose->setEnabled(p);
    m_actProps->setEnabled(has);
    m_actCopy->setEnabled(sel);
    m_copyAsMenu->setEnabled(sel);
    for (QAction *a : { m_actCopyHex, m_actCopyHexCompact, m_actCopyBin, m_actCopyC, m_actCopyText })
        a->setEnabled(sel);
    m_actPaste->setEnabled(editable);
    m_actPasteInsert->setEnabled(editable);
    m_actInsert->setEnabled(editable);
    m_actFill->setEnabled(editable && sel);
    m_actDelete->setEnabled(editable && sel);
    m_actSelectAll->setEnabled(has);
    m_actSelectRange->setEnabled(has);
    m_actEditMode->setEnabled(ep);
    m_actEditMode->setChecked(editable);
    m_actEditMode->setText(editable ? tr("&Edit Mode (on)") : tr("&Edit Mode"));
    for (QAction *a : { m_actFind, m_actFindNext, m_actFindPrev, m_actGoto, m_actChecksum, m_actZoomIn, m_actZoomOut,
                        m_actZoomReset })
        a->setEnabled(has);
    m_actReplace->setEnabled(editable);
    m_actBase->setEnabled(ep);
    const bool cmp = qobject_cast<ComparePage *>(p) != nullptr;
    m_actNextDiff->setEnabled(cmp);
    m_actPrevDiff->setEnabled(cmp);
}

void MainWindow::updateStatus()
{
    HexView *v = currentView();
    Document *d = v ? v->document() : nullptr;
    m_inspector->setSource(d, v ? v->cursorPosition() : 0);
    if (!d) {
        for (QLabel *l : { m_stPos, m_stValue, m_stSel, m_stSize, m_stFormat })
            l->clear();
        m_stMode->hide();
        return;
    }
    const qint64 pos = v->cursorPosition();
    m_stPos->setText(tr("Offset: 0x%1 (%2)").arg(hexStr(quint64(pos)), QLocale().toString(pos))
                     + QStringLiteral("   ") + tr("Address: 0x%1").arg(hexStr(d->baseAddress() + quint64(pos), 8)));
    if (pos < d->size()) {
        const int unit = v->asciiPaneActive() ? 1 : v->unitSize();
        const qint64 off = pos - pos % unit;
        int avail = 0;
        quint64 val = 0;
        if (unit == 1) {
            val = d->byteAt(off);
            avail = 1;
        } else {
            val = v->unitValue(off, &avail);
        }
        if (avail == unit) {
            m_stValue->setText(tr("Value: 0x%1 = %2 = 0o%3")
                                   .arg(hexStr(val, unit * 2), QString::number(val), QString::number(val, 8)));
        } else {
            m_stValue->setText(tr("Value: incomplete unit"));
        }
    } else {
        m_stValue->setText(tr("End of file"));
    }
    m_stSel->setText(v->hasSelection() ? tr("Selected: %1 bytes (0x%2)").arg(QLocale().toString(v->selectionLength()),
                                                                               hexStr(quint64(v->selectionLength())))
                                       : QString());
    m_stSize->setText(tr("Size: %1 bytes").arg(QLocale().toString(d->size())));
    m_stFormat->setText(Formats::formatName(d->format()));
    m_stMode->show();
    m_stMode->setEnabled(currentEditor() != nullptr);
    m_stMode->setText(d->isReadOnly() ? tr("Read-only") : tr("Edit"));
}

void MainWindow::updateTitle()
{
    TabPage *p = currentPage();
    setWindowTitle(p ? QStringLiteral("%1 - qBinEdit").arg(p->title()) : QStringLiteral("qBinEdit"));
}

void MainWindow::statusMessage(const QString &msg)
{
    statusBar()->showMessage(msg, 6000);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto *ep = qobject_cast<EditorPage *>(m_tabs->widget(i))) {
            if (!maybeSave(ep)) {
                e->ignore();
                return;
            }
        }
    }
    saveSettings();
    if (m_find)
        m_find->close();
    e->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls())
        e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *e)
{
    const auto urls = e->mimeData()->urls();
    for (const QUrl &u : urls)
        if (u.isLocalFile())
            openFile(u.toLocalFile());
    e->acceptProposedAction();
}
