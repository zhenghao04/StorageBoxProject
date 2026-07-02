#pragma once

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMenu>
#include <QPoint>
#include <QPointer>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QSystemTrayIcon>
#include <QWidget>

class QDragEnterEvent;
class QDragLeaveEvent;
class QDropEvent;
class QEvent;
class QGridLayout;
class QLabel;
class QMimeData;
class BoxPopup;
class BoxWindow;

struct LaunchItem
{
    QString name;
    QString path;
};

struct Box
{
    QString id;
    QString name;
    QPoint position;
    QSize size;
    QColor color;
    QString iconPath;
    QList<LaunchItem> items;
};

class StorageBoxApp : public QObject
{
    Q_OBJECT

public:
    explicit StorageBoxApp(QObject *parent = nullptr);
    ~StorageBoxApp() override;

    void start();
    void save();
    void showPopup(BoxWindow *window);
    void closePopup();
    void addBox();
    void renameBox(Box *box, QWidget *parent);
    void deleteBox(Box *box, QWidget *parent);
    void setBoxColor(Box *box, const QColor &color);
    void chooseBoxColor(Box *box, QWidget *parent);
    void chooseBoxIcon(Box *box, QWidget *parent);
    void clearBoxIcon(Box *box);
    void addApp(Box *box, QWidget *parent);
    void addFolder(Box *box, QWidget *parent);
    void renameApp(Box *box, int index, QWidget *parent);
    void removeApp(Box *box, int index, QWidget *parent);
    void addDroppedPaths(Box *box, const QStringList &paths, int insertIndex, QWidget *parent);
    void moveApp(Box *box, int sourceIndex, int targetIndex);
    void launchApp(const LaunchItem &item, QWidget *parent);
    void setAlwaysOnTop(bool enabled);
    bool alwaysOnTop() const;
    void showSettings(QWidget *parent);
    void setThemeId(const QString &themeId);
    QString themeId() const;
    void openConfigFolder(QWidget *parent) const;
    bool startAtLogin() const;
    bool setStartAtLogin(bool enabled, QWidget *parent);
    int boxIndex(Box *box) const;

private:
    void load();
    void ensureDefaults();
    void renderBoxes(bool persist = true);
    void refreshViews(Box *box = nullptr);
    void setupTray();
    void closePopupNow(bool animated);
    QString configFilePath() const;
    QString configBackupFilePath() const;
    QString iconStorageDirPath() const;
    QString startupShortcutPath() const;
    void backupConfigFile(const QString &label) const;
    QString copyIconToStorage(const QString &sourcePath, const QString &boxId) const;
    QColor colorForIndex(int index) const;
    QString defaultNameForPath(const QString &path) const;
    QJsonObject itemToJson(const LaunchItem &item) const;
    QJsonObject boxToJson(const Box &box) const;
    LaunchItem itemFromJson(const QJsonObject &json) const;
    Box boxFromJson(const QJsonObject &json, int index) const;

    QList<Box> m_boxes;
    QList<BoxWindow *> m_windows;
    QPointer<BoxPopup> m_popup;
    QSystemTrayIcon *m_tray = nullptr;
    bool m_alwaysOnTop = false;
    bool m_configNeedsSave = false;
    QString m_themeId;
};

class BoxWindow : public QWidget
{
    Q_OBJECT

public:
    BoxWindow(StorageBoxApp *app, Box *box);
    Box *box() const;
    void refresh();

protected:
    bool event(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    enum class HitRegion
    {
        None,
        Move,
        Left,
        Right,
        Top,
        Bottom,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
    };

    void applyWindowFlags();
    HitRegion hitRegionAt(const QPoint &point) const;
    void updateCursor(const QPoint &point);
    void applyResize(const QPoint &delta);

    StorageBoxApp *m_app;
    Box *m_box;
    QPoint m_pressGlobal;
    QPoint m_startPosition;
    QSize m_startSize;
    HitRegion m_dragRegion = HitRegion::None;
    bool m_dragged = false;
    bool m_hovered = false;
    bool m_dropActive = false;
};

class BoxPopup : public QWidget
{
    Q_OBJECT

public:
    BoxPopup(StorageBoxApp *app, BoxWindow *boxWindow);
    Box *box() const;
    void refresh();
    void showAnimated();
    void closeAnimated();
    void startItemDrag(int index, QWidget *source);
    bool handleDropOnSlot(int targetIndex, const QMimeData *mimeData);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void buildUi();
    void updateHeader();
    void rebuildGrid();
    void buildItemButton(int index);
    void showAddMenu(QWidget *anchor);
    void showItemMenu(const QPoint &globalPos, int index);
    void applyWindowFlags();

    StorageBoxApp *m_app;
    BoxWindow *m_boxWindow;
    QGridLayout *m_grid = nullptr;
    QLabel *m_iconLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_countLabel = nullptr;
    bool m_closing = false;
};
