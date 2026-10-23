// qBinEdit - main window
#pragma once

#include "formats.h"

#include <QMainWindow>
#include <QPointer>

class ComparePage;
class DataInspector;
class Document;
class EditorPage;
class FindDialog;
class HexView;
class QAction;
class QActionGroup;
class QComboBox;
class QDockWidget;
class QLabel;
class QMenu;
class QStackedWidget;
class QTabWidget;
class QToolButton;
class QUndoGroup;
class TabPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    bool openFile(const QString &path, int forceFormat = -1);
    bool compareFiles(const QString &left, const QString &right);
    void setInitialViewOptions(int radix, int unitBytes, int bigEndian);   // from the command line; -1 = keep

protected:
    void closeEvent(QCloseEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;

private:
    // construction
    void createActions();
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void createDock();
    void loadSettings();
    void saveSettings();

    // tabs
    TabPage *currentPage() const;
    EditorPage *currentEditor() const;
    HexView *currentView() const;
    Document *currentDocument() const;
    void addPage(TabPage *page);
    void wireView(HexView *v);
    void applyDefaults(HexView *v);
    bool closeTab(int index);
    bool maybeSave(EditorPage *page);
    void onCurrentTabChanged();

    // file
    void newFile();
    void openDialog();
    void openRawDialog();
    bool save();
    bool saveAs();
    bool saveDocument(Document *doc, bool forceDialog);
    void showProperties();
    void addRecent(const QString &path);
    void updateRecentMenu();

    // edit
    void toggleEditMode(bool editable);
    void copyAs(int kind);
    void paste(bool insert);
    void deleteSelection();
    void insertBytes();
    void fillSelection();
    void selectRange();

    // search & navigation
    void showFind(bool replace);
    void findNext(bool backward);
    void countMatches();
    void replaceOne();
    void replaceAll();
    void gotoOffset();
    void setBaseAddress();

    // tools
    void showChecksum();
    void compareWith();
    void nextDifference(bool backward);

    // view
    void setRadix(int radix);
    void setUnit(int bytes);
    void setBigEndian(bool big);
    void setBytesPerRow(int n);
    void setShowAscii(bool on);
    void setDimFill(bool on);
    void zoom(int steps);
    void chooseFont();
    void setLanguage(const QString &code);
    void syncViewControls();

    void about();
    void showContextMenu(const QPoint &globalPos);
    void updateActions();
    void updateStatus();
    void updateTitle();
    void statusMessage(const QString &msg);

    // widgets
    QStackedWidget *m_stack = nullptr;
    QTabWidget *m_tabs = nullptr;
    QLabel *m_welcome = nullptr;
    QDockWidget *m_inspectorDock = nullptr;
    DataInspector *m_inspector = nullptr;
    QPointer<FindDialog> m_find;
    QUndoGroup *m_undoGroup = nullptr;

    QComboBox *m_radixCombo = nullptr;
    QComboBox *m_unitCombo = nullptr;
    QComboBox *m_endianCombo = nullptr;
    QComboBox *m_bprCombo = nullptr;

    QLabel *m_stPos = nullptr;
    QLabel *m_stValue = nullptr;
    QLabel *m_stSel = nullptr;
    QLabel *m_stSize = nullptr;
    QLabel *m_stFormat = nullptr;
    QToolButton *m_stMode = nullptr;

    // actions
    QAction *m_actNew, *m_actOpen, *m_actOpenRaw, *m_actSave, *m_actSaveAs, *m_actClose, *m_actProps, *m_actExit;
    QAction *m_actUndo, *m_actRedo, *m_actCopy, *m_actCopyHex, *m_actCopyHexCompact, *m_actCopyBin, *m_actCopyC,
        *m_actCopyText, *m_actPaste, *m_actPasteInsert, *m_actSelectAll, *m_actSelectRange, *m_actFill, *m_actInsert,
        *m_actDelete, *m_actEditMode;
    QAction *m_actFind, *m_actReplace, *m_actFindNext, *m_actFindPrev, *m_actGoto, *m_actBase;
    QAction *m_actChecksum, *m_actCompare, *m_actNextDiff, *m_actPrevDiff;
    QAction *m_actAscii, *m_actDim, *m_actZoomIn, *m_actZoomOut, *m_actZoomReset, *m_actFont, *m_actInspector;
    QAction *m_actAbout, *m_actAboutQt;
    QActionGroup *m_radixGroup, *m_unitGroup, *m_endianGroup, *m_bprGroup, *m_langGroup;
    QMenu *m_recentMenu = nullptr;
    QMenu *m_copyAsMenu = nullptr;

    // defaults for new views (persisted)
    int m_defRadix = 16;
    int m_defUnit = 1;
    bool m_defBig = false;
    int m_defBpr = 16;
    bool m_defAscii = true;
    bool m_defDim = true;
    QString m_defFont;
    QString m_lastDir;
    bool m_syncing = false;
    int m_untitled = 0;
    int m_bprBeforeBinary = 0;
};
