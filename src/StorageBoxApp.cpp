#include "StorageBoxApp.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSize>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

namespace {
constexpr int kMaxItemsPerBox = 9;
constexpr int kBoxSize = 78;
constexpr int kMinBoxSize = 58;
constexpr int kMaxBoxSize = 180;
constexpr int kResizeGrip = 7;
constexpr int kCellWidth = 116;
constexpr int kCellHeight = 78;
constexpr const char *kItemIndexMime = "application/x-storagebox-item-index";
constexpr const char *kBoxIdMime = "application/x-storagebox-id";

QString makeId()
{
    return QString::number(QDateTime::currentMSecsSinceEpoch(), 16)
        + QString::number(QRandomGenerator::global()->generate(), 16);
}

bool mimeHasLocalFiles(const QMimeData *mimeData)
{
    if (!mimeData || !mimeData->hasUrls()) {
        return false;
    }

    for (const QUrl &url : mimeData->urls()) {
        if (url.isLocalFile() && QFileInfo::exists(url.toLocalFile())) {
            return true;
        }
    }
    return false;
}

QStringList localFilesFromMime(const QMimeData *mimeData)
{
    QStringList paths;
    if (!mimeData) {
        return paths;
    }

    for (const QUrl &url : mimeData->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString path = QDir::toNativeSeparators(url.toLocalFile());
        if (QFileInfo::exists(path) && !paths.contains(path, Qt::CaseInsensitive)) {
            paths.append(path);
        }
    }
    return paths;
}

class SlotButton : public QToolButton
{
public:
    SlotButton(BoxPopup *popup, int index, bool occupied, QWidget *parent)
        : QToolButton(parent), m_popup(popup), m_index(index), m_occupied(occupied)
    {
        setAcceptDrops(true);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_dragStart = event->pos();
        }
        QToolButton::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_occupied
            && (event->buttons() & Qt::LeftButton)
            && (event->pos() - m_dragStart).manhattanLength() >= QApplication::startDragDistance()) {
            setDown(false);
            m_popup->startItemDrag(m_index, this);
            return;
        }
        QToolButton::mouseMoveEvent(event);
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        const QMimeData *mimeData = event->mimeData();
        if ((mimeData && mimeData->hasFormat(kItemIndexMime)) || mimeHasLocalFiles(mimeData)) {
            event->acceptProposedAction();
            return;
        }
        QToolButton::dragEnterEvent(event);
    }

    void dropEvent(QDropEvent *event) override
    {
        if (m_popup->handleDropOnSlot(m_index, event->mimeData())) {
            event->acceptProposedAction();
            return;
        }
        QToolButton::dropEvent(event);
    }

private:
    BoxPopup *m_popup;
    int m_index;
    bool m_occupied;
    QPoint m_dragStart;
};

QIcon iconForPath(const QString &path)
{
#ifdef Q_OS_WIN
    SHFILEINFOW info = {};
    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    const DWORD_PTR result = SHGetFileInfoW(
        nativePath.c_str(),
        FILE_ATTRIBUTE_NORMAL,
        &info,
        sizeof(info),
        SHGFI_ICON | SHGFI_LARGEICON);

    if (result != 0 && info.hIcon) {
        const QImage image = QImage::fromHICON(info.hIcon);
        DestroyIcon(info.hIcon);
        if (!image.isNull()) {
            return QIcon(QPixmap::fromImage(image));
        }
    }
#endif

    QFileIconProvider provider;
    const QIcon fileIcon = provider.icon(QFileInfo(path));
    return fileIcon.isNull() ? QApplication::windowIcon() : fileIcon;
}

void ensureWidgetNonTopmost(QWidget *widget)
{
    if (!widget) {
        return;
    }

#ifdef Q_OS_WIN
    SetWindowPos(
        reinterpret_cast<HWND>(widget->winId()),
        HWND_NOTOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
#else
    Q_UNUSED(widget);
#endif
}

void restoreWidgetLayer(QWidget *widget)
{
#ifdef Q_OS_WIN
    if (widget) {
        SetWindowPos(
            reinterpret_cast<HWND>(widget->winId()),
            HWND_TOP,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
#else
    Q_UNUSED(widget);
#endif
}

QString elide(const QString &text, int maxChars)
{
    if (text.size() <= maxChars) {
        return text;
    }
    return text.left(maxChars - 1) + QStringLiteral("…");
}
}

StorageBoxApp::StorageBoxApp(QObject *parent)
    : QObject(parent)
{
}

StorageBoxApp::~StorageBoxApp()
{
    closePopup();
    qDeleteAll(m_windows);
}

void StorageBoxApp::start()
{
    load();
    setupTray();
    renderBoxes();
}

void StorageBoxApp::load()
{
    QFile file(configFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        ensureDefaults();
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = document.object();
    m_alwaysOnTop = root.value(QStringLiteral("alwaysOnTop")).toBool(false);

    const QJsonArray boxes = root.value(QStringLiteral("boxes")).toArray();
    m_boxes.clear();
    for (int i = 0; i < boxes.size(); ++i) {
        const QJsonObject object = boxes.at(i).toObject();
        if (!object.isEmpty()) {
            m_boxes.append(boxFromJson(object, i));
        }
    }

    ensureDefaults();
}

void StorageBoxApp::ensureDefaults()
{
    if (!m_boxes.isEmpty()) {
        return;
    }

    m_boxes.append(Box{
        makeId(),
        QStringLiteral("收纳盒 1"),
        QPoint(96, 160),
        QSize(kBoxSize, kBoxSize),
        QColor(QStringLiteral("#2563eb")),
        {},
    });
}

void StorageBoxApp::save()
{
    QJsonObject root;
    root.insert(QStringLiteral("alwaysOnTop"), m_alwaysOnTop);

    QJsonArray boxes;
    for (const Box &box : m_boxes) {
        boxes.append(boxToJson(box));
    }
    root.insert(QStringLiteral("boxes"), boxes);

    QFileInfo info(configFilePath());
    QDir().mkpath(info.absolutePath());

    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void StorageBoxApp::renderBoxes()
{
    closePopup();
    for (BoxWindow *window : qAsConst(m_windows)) {
        window->close();
        window->deleteLater();
    }
    m_windows.clear();

    for (Box &box : m_boxes) {
        auto *window = new BoxWindow(this, &box);
        m_windows.append(window);
        window->show();
        if (!m_alwaysOnTop) {
            QTimer::singleShot(0, window, [window] { ensureWidgetNonTopmost(window); });
        }
    }

    save();
}

void StorageBoxApp::refreshViews(Box *box)
{
    for (BoxWindow *window : qAsConst(m_windows)) {
        if (!box || window->box() == box) {
            window->refresh();
        }
    }

    if (m_popup && (!box || m_popup->box() == box)) {
        m_popup->refresh();
    }

    save();
}

void StorageBoxApp::setupTray()
{
    if (m_tray) {
        return;
    }

    m_tray = new QSystemTrayIcon(QApplication::windowIcon(), this);
    auto *menu = new QMenu;
    menu->addAction(QStringLiteral("新建盒子"), this, &StorageBoxApp::addBox);
    menu->addSeparator();

    QAction *topmost = menu->addAction(QStringLiteral("置顶显示"));
    topmost->setCheckable(true);
    topmost->setChecked(m_alwaysOnTop);
    connect(topmost, &QAction::toggled, this, &StorageBoxApp::setAlwaysOnTop);

    menu->addSeparator();
    menu->addAction(QStringLiteral("退出"), qApp, &QApplication::quit);
    m_tray->setContextMenu(menu);
    m_tray->setToolTip(QStringLiteral("Storage Box Launcher"));
    m_tray->show();
}

QString StorageBoxApp::configFilePath() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(base).filePath(QStringLiteral("config.json"));
}

void StorageBoxApp::showPopup(BoxWindow *window)
{
    if (m_popup && m_popup->box() == window->box()) {
        closePopup();
        return;
    }

    closePopup();
    m_popup = new BoxPopup(this, window);
    m_popup->show();
    if (!m_alwaysOnTop) {
        BoxPopup *popup = m_popup.data();
        QTimer::singleShot(0, popup, [popup] { ensureWidgetNonTopmost(popup); });
    }
}

void StorageBoxApp::closePopup()
{
    if (!m_popup) {
        return;
    }
    m_popup->close();
    m_popup->deleteLater();
    m_popup = nullptr;
}

void StorageBoxApp::addBox()
{
    const int count = m_boxes.size() + 1;
    bool ok = false;
    const QString name = QInputDialog::getText(
        nullptr,
        QStringLiteral("新建收纳盒"),
        QStringLiteral("盒子名称："),
        QLineEdit::Normal,
        QStringLiteral("收纳盒 %1").arg(count),
        &ok);
    if (!ok) {
        return;
    }

    const int offset = 24 * (count - 1);
    m_boxes.append(Box{
        makeId(),
        name.trimmed().isEmpty() ? QStringLiteral("收纳盒 %1").arg(count) : name.trimmed(),
        QPoint(96 + offset, 160 + offset),
        QSize(kBoxSize, kBoxSize),
        colorForIndex(count - 1),
        {},
    });
    renderBoxes();
}

void StorageBoxApp::renameBox(Box *box, QWidget *parent)
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        parent,
        QStringLiteral("重命名盒子"),
        QStringLiteral("盒子名称："),
        QLineEdit::Normal,
        box->name,
        &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    box->name = name.trimmed();
    renderBoxes();
}

void StorageBoxApp::deleteBox(Box *box, QWidget *parent)
{
    if (m_boxes.size() <= 1) {
        QMessageBox::information(parent, QStringLiteral("Storage Box Launcher"), QStringLiteral("至少需要保留一个收纳盒。"));
        return;
    }

    if (QMessageBox::question(parent, QStringLiteral("删除盒子"), QStringLiteral("删除「%1」吗？").arg(box->name)) != QMessageBox::Yes) {
        return;
    }

    const int index = boxIndex(box);
    if (index >= 0) {
        m_boxes.removeAt(index);
        renderBoxes();
    }
}

void StorageBoxApp::addApp(Box *box, QWidget *parent)
{
    if (box->items.size() >= kMaxItemsPerBox) {
        QMessageBox::information(parent, QStringLiteral("盒子已满"), QStringLiteral("每个盒子最多收纳 9 个应用。"));
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        parent,
        QStringLiteral("选择应用或快捷方式"),
        QString(),
        QStringLiteral("可启动文件 (*.exe *.lnk *.bat *.cmd *.com *.url);;所有文件 (*.*)"));
    if (path.isEmpty()) {
        return;
    }

    bool ok = false;
    const QString defaultName = defaultNameForPath(path);
    QString name = QInputDialog::getText(
        parent,
        QStringLiteral("显示名称"),
        QStringLiteral("应用名称："),
        QLineEdit::Normal,
        defaultName,
        &ok);
    if (!ok || name.trimmed().isEmpty()) {
        name = defaultName;
    }

    box->items.append(LaunchItem{name.trimmed(), path});
    renderBoxes();
}

void StorageBoxApp::renameApp(Box *box, int index, QWidget *parent)
{
    if (index < 0 || index >= box->items.size()) {
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(
        parent,
        QStringLiteral("重命名应用"),
        QStringLiteral("应用名称："),
        QLineEdit::Normal,
        box->items.at(index).name,
        &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    box->items[index].name = name.trimmed();
    renderBoxes();
}

void StorageBoxApp::removeApp(Box *box, int index, QWidget *parent)
{
    if (index < 0 || index >= box->items.size()) {
        return;
    }

    const QString name = box->items.at(index).name;
    if (QMessageBox::question(parent, QStringLiteral("移除应用"), QStringLiteral("从盒子里移除「%1」吗？").arg(name)) != QMessageBox::Yes) {
        return;
    }

    box->items.removeAt(index);
    renderBoxes();
}

void StorageBoxApp::addDroppedPaths(Box *box, const QStringList &paths, int insertIndex, QWidget *parent)
{
    if (!box || paths.isEmpty()) {
        return;
    }

    int added = 0;
    for (const QString &path : paths) {
        if (box->items.size() >= kMaxItemsPerBox) {
            break;
        }

        bool exists = false;
        for (const LaunchItem &item : qAsConst(box->items)) {
            if (QString::compare(item.path, path, Qt::CaseInsensitive) == 0) {
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }

        const LaunchItem item{defaultNameForPath(path), path};
        if (insertIndex >= 0) {
            const int target = qBound(0, insertIndex + added, box->items.size());
            box->items.insert(target, item);
        } else {
            box->items.append(item);
        }
        ++added;
    }

    if (added == 0 && box->items.size() >= kMaxItemsPerBox) {
        QMessageBox::information(parent, QStringLiteral("盒子已满"), QStringLiteral("每个盒子最多收纳 9 个应用。"));
        return;
    }

    if (added > 0) {
        refreshViews(box);
    }
}

void StorageBoxApp::moveApp(Box *box, int sourceIndex, int targetIndex)
{
    if (!box || sourceIndex < 0 || sourceIndex >= box->items.size() || targetIndex < 0) {
        return;
    }

    LaunchItem item = box->items.takeAt(sourceIndex);
    const int target = qBound(0, targetIndex, box->items.size());
    box->items.insert(target, item);
    refreshViews(box);
}

void StorageBoxApp::launchApp(const LaunchItem &item, QWidget *parent)
{
    if (!QFileInfo::exists(item.path)) {
        QMessageBox::warning(parent, QStringLiteral("无法启动"), QStringLiteral("找不到文件：\n%1").arg(item.path));
        return;
    }

    closePopup();

#ifdef Q_OS_WIN
    const std::wstring path = QDir::toNativeSeparators(item.path).toStdWString();
    const HINSTANCE result = ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        QMessageBox::warning(parent, QStringLiteral("启动失败"), QStringLiteral("Windows 无法打开：\n%1").arg(item.path));
    }
#else
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(item.path))) {
        QMessageBox::warning(parent, QStringLiteral("启动失败"), QStringLiteral("无法打开：\n%1").arg(item.path));
    }
#endif
}

void StorageBoxApp::setAlwaysOnTop(bool enabled)
{
    m_alwaysOnTop = enabled;
    for (BoxWindow *window : qAsConst(m_windows)) {
        window->refresh();
    }
    if (m_popup) {
        m_popup->refresh();
    }
    save();
}

bool StorageBoxApp::alwaysOnTop() const
{
    return m_alwaysOnTop;
}

int StorageBoxApp::boxIndex(Box *box) const
{
    for (int i = 0; i < m_boxes.size(); ++i) {
        if (&m_boxes[i] == box) {
            return i;
        }
    }
    return -1;
}

QColor StorageBoxApp::colorForIndex(int index) const
{
    static const QList<QColor> colors = {
        QColor(QStringLiteral("#2563eb")),
        QColor(QStringLiteral("#059669")),
        QColor(QStringLiteral("#d97706")),
        QColor(QStringLiteral("#7c3aed")),
        QColor(QStringLiteral("#dc2626")),
        QColor(QStringLiteral("#0891b2")),
    };
    return colors.at(index % colors.size());
}

QString StorageBoxApp::defaultNameForPath(const QString &path) const
{
    const QFileInfo info(path);
    return info.completeBaseName().isEmpty() ? info.fileName() : info.completeBaseName();
}

QJsonObject StorageBoxApp::itemToJson(const LaunchItem &item) const
{
    QJsonObject object;
    object.insert(QStringLiteral("name"), item.name);
    object.insert(QStringLiteral("path"), item.path);
    return object;
}

QJsonObject StorageBoxApp::boxToJson(const Box &box) const
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), box.id);
    object.insert(QStringLiteral("name"), box.name);
    object.insert(QStringLiteral("x"), box.position.x());
    object.insert(QStringLiteral("y"), box.position.y());
    object.insert(QStringLiteral("width"), box.size.width());
    object.insert(QStringLiteral("height"), box.size.height());
    object.insert(QStringLiteral("color"), box.color.name());

    QJsonArray items;
    for (const LaunchItem &item : box.items) {
        items.append(itemToJson(item));
    }
    object.insert(QStringLiteral("items"), items);
    return object;
}

LaunchItem StorageBoxApp::itemFromJson(const QJsonObject &json) const
{
    const QString path = json.value(QStringLiteral("path")).toString();
    return LaunchItem{
        json.value(QStringLiteral("name")).toString(defaultNameForPath(path)),
        path,
    };
}

Box StorageBoxApp::boxFromJson(const QJsonObject &json, int index) const
{
    Box box;
    box.id = json.value(QStringLiteral("id")).toString(makeId());
    box.name = json.value(QStringLiteral("name")).toString(QStringLiteral("收纳盒 %1").arg(index + 1));
    box.position = QPoint(json.value(QStringLiteral("x")).toInt(96 + index * 24), json.value(QStringLiteral("y")).toInt(160 + index * 24));
    box.size = QSize(
        qBound(kMinBoxSize, json.value(QStringLiteral("width")).toInt(kBoxSize), kMaxBoxSize),
        qBound(kMinBoxSize, json.value(QStringLiteral("height")).toInt(kBoxSize), kMaxBoxSize));
    box.color = QColor(json.value(QStringLiteral("color")).toString(colorForIndex(index).name()));

    const QJsonArray items = json.value(QStringLiteral("items")).toArray();
    for (int i = 0; i < items.size() && box.items.size() < kMaxItemsPerBox; ++i) {
        const LaunchItem item = itemFromJson(items.at(i).toObject());
        if (!item.path.isEmpty()) {
            box.items.append(item);
        }
    }
    return box;
}

BoxWindow::BoxWindow(StorageBoxApp *app, Box *box)
    : QWidget(nullptr), m_app(app), m_box(box)
{
    setMinimumSize(kMinBoxSize, kMinBoxSize);
    setMaximumSize(kMaxBoxSize, kMaxBoxSize);
    resize(m_box->size);
    setWindowIcon(QApplication::windowIcon());
    setAcceptDrops(true);
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("左键打开，拖动移动，拖边缩放，右键管理，双击添加应用"));
    applyWindowFlags();
    move(m_box->position);
}

Box *BoxWindow::box() const
{
    return m_box;
}

void BoxWindow::refresh()
{
    applyWindowFlags();
    resize(m_box->size);
    move(m_box->position);
    update();
    show();
    if (!m_app->alwaysOnTop()) {
        QTimer::singleShot(0, this, [this] { ensureWidgetNonTopmost(this); });
    } else {
        restoreWidgetLayer(this);
    }
}

void BoxWindow::applyWindowFlags()
{
    if (m_app->alwaysOnTop()) {
        restoreWidgetLayer(this);
    }

    Qt::WindowFlags flags = Qt::FramelessWindowHint | Qt::Tool;
    if (m_app->alwaysOnTop()) {
        flags |= Qt::WindowStaysOnTopHint;
    }
    setWindowFlags(flags);
    setAttribute(Qt::WA_TranslucentBackground, true);
}

void BoxWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_pressGlobal = event->globalPos();
    m_startPosition = pos();
    m_startSize = size();
    m_dragRegion = hitRegionAt(event->pos());
    if (m_dragRegion != HitRegion::Move) {
        m_app->closePopup();
    }
    m_dragged = false;
}

void BoxWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        updateCursor(event->pos());
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPoint delta = event->globalPos() - m_pressGlobal;
    if (delta.manhattanLength() > 4) {
        m_dragged = true;
    }

    if (m_dragRegion == HitRegion::Move || m_dragRegion == HitRegion::None) {
        const QPoint next(qMax(0, m_startPosition.x() + delta.x()), qMax(0, m_startPosition.y() + delta.y()));
        move(next);
        m_box->position = next;
    } else {
        applyResize(delta);
    }
}

void BoxWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_app->save();
        if (!m_dragged && m_dragRegion == HitRegion::Move) {
            m_app->showPopup(this);
        }
        m_dragRegion = HitRegion::None;
        updateCursor(event->pos());
        if (!m_app->alwaysOnTop()) {
            QTimer::singleShot(0, this, [this] { ensureWidgetNonTopmost(this); });
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void BoxWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_app->addApp(m_box, this);
    }
}

void BoxWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.addAction(QStringLiteral("打开"), this, [this] { m_app->showPopup(this); });
    menu.addAction(QStringLiteral("添加应用"), this, [this] { m_app->addApp(m_box, this); });
    menu.addAction(QStringLiteral("重命名盒子"), this, [this] { m_app->renameBox(m_box, this); });
    menu.addSeparator();

    QAction *topmost = menu.addAction(QStringLiteral("置顶显示"));
    topmost->setCheckable(true);
    topmost->setChecked(m_app->alwaysOnTop());
    connect(topmost, &QAction::toggled, m_app, &StorageBoxApp::setAlwaysOnTop);

    menu.addAction(QStringLiteral("新建盒子"), m_app, &StorageBoxApp::addBox);
    menu.addAction(QStringLiteral("删除这个盒子"), this, [this] { m_app->deleteBox(m_box, this); });
    menu.addSeparator();
    menu.addAction(QStringLiteral("退出"), qApp, &QApplication::quit);
    menu.exec(event->globalPos());
    if (!m_app->alwaysOnTop()) {
        QTimer::singleShot(0, this, [this] { ensureWidgetNonTopmost(this); });
    }
}

void BoxWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (mimeHasLocalFiles(event->mimeData())) {
        event->acceptProposedAction();
        return;
    }
    QWidget::dragEnterEvent(event);
}

void BoxWindow::dropEvent(QDropEvent *event)
{
    const QStringList paths = localFilesFromMime(event->mimeData());
    if (!paths.isEmpty()) {
        m_app->addDroppedPaths(m_box, paths, -1, this);
        event->acceptProposedAction();
        if (!m_app->alwaysOnTop()) {
            QTimer::singleShot(0, this, [this] { ensureWidgetNonTopmost(this); });
        }
        return;
    }
    QWidget::dropEvent(event);
}

BoxWindow::HitRegion BoxWindow::hitRegionAt(const QPoint &point) const
{
    const bool left = point.x() <= kResizeGrip;
    const bool right = point.x() >= width() - kResizeGrip;
    const bool top = point.y() <= kResizeGrip;
    const bool bottom = point.y() >= height() - kResizeGrip;

    if (top && left) {
        return HitRegion::TopLeft;
    }
    if (top && right) {
        return HitRegion::TopRight;
    }
    if (bottom && left) {
        return HitRegion::BottomLeft;
    }
    if (bottom && right) {
        return HitRegion::BottomRight;
    }
    if (left) {
        return HitRegion::Left;
    }
    if (right) {
        return HitRegion::Right;
    }
    if (top) {
        return HitRegion::Top;
    }
    if (bottom) {
        return HitRegion::Bottom;
    }
    return HitRegion::Move;
}

void BoxWindow::updateCursor(const QPoint &point)
{
    switch (hitRegionAt(point)) {
    case HitRegion::Left:
    case HitRegion::Right:
        setCursor(Qt::SizeHorCursor);
        break;
    case HitRegion::Top:
    case HitRegion::Bottom:
        setCursor(Qt::SizeVerCursor);
        break;
    case HitRegion::TopLeft:
    case HitRegion::BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case HitRegion::TopRight:
    case HitRegion::BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    default:
        setCursor(Qt::PointingHandCursor);
        break;
    }
}

void BoxWindow::applyResize(const QPoint &delta)
{
    QRect next(m_startPosition, m_startSize);

    switch (m_dragRegion) {
    case HitRegion::Left:
    case HitRegion::TopLeft:
    case HitRegion::BottomLeft: {
        const int newWidth = qBound(kMinBoxSize, m_startSize.width() - delta.x(), kMaxBoxSize);
        next.setLeft(m_startPosition.x() + (m_startSize.width() - newWidth));
        next.setWidth(newWidth);
        break;
    }
    case HitRegion::Right:
    case HitRegion::TopRight:
    case HitRegion::BottomRight:
        next.setWidth(qBound(kMinBoxSize, m_startSize.width() + delta.x(), kMaxBoxSize));
        break;
    default:
        break;
    }

    switch (m_dragRegion) {
    case HitRegion::Top:
    case HitRegion::TopLeft:
    case HitRegion::TopRight: {
        const int newHeight = qBound(kMinBoxSize, m_startSize.height() - delta.y(), kMaxBoxSize);
        next.setTop(m_startPosition.y() + (m_startSize.height() - newHeight));
        next.setHeight(newHeight);
        break;
    }
    case HitRegion::Bottom:
    case HitRegion::BottomLeft:
    case HitRegion::BottomRight:
        next.setHeight(qBound(kMinBoxSize, m_startSize.height() + delta.y(), kMaxBoxSize));
        break;
    default:
        break;
    }

    next.moveTopLeft(QPoint(qMax(0, next.x()), qMax(0, next.y())));
    setGeometry(next);
    m_box->position = next.topLeft();
    m_box->size = next.size();
}

void BoxWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int side = qMin(width(), height());
    QRectF outer(2, 2, width() - 4, height() - 4);
    QPainterPath path;
    const qreal radius = qBound(10.0, side / 5.0, 24.0);
    path.addRoundedRect(outer, radius, radius);

    painter.fillPath(path, m_box->color);
    painter.setPen(QPen(QColor(255, 255, 255, 220), 2));
    painter.drawPath(path);

    painter.setPen(Qt::white);
    QFont nameFont(QStringLiteral("Microsoft YaHei UI"), 9, QFont::DemiBold);
    nameFont.setPointSize(qBound(7, side / 8, 12));
    painter.setFont(nameFont);
    painter.drawText(
        QRect(8, qMax(6, height() / 9), width() - 16, qMax(18, height() / 3)),
        Qt::AlignCenter | Qt::TextWordWrap,
        elide(m_box->name, qBound(6, width() / 10, 14)));

    QFont countFont(QStringLiteral("Segoe UI"), 11, QFont::Bold);
    countFont.setPointSize(qBound(9, side / 7, 18));
    painter.setFont(countFont);
    painter.drawText(
        QRect(0, height() / 2, width(), qMax(18, height() / 4)),
        Qt::AlignCenter,
        QStringLiteral("%1/9").arg(m_box->items.size()));

    const qreal spacing = qMax(7.0, side / 6.0);
    const qreal dotRadius = qBound(1.8, side / 34.0, 4.0);
    const QPointF dotCenter(width() / 2.0, height() * 0.76);
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < kMaxItemsPerBox; ++i) {
        const int row = i / 3;
        const int col = i % 3;
        painter.setBrush(i < m_box->items.size() ? QColor("#ffffff") : QColor(255, 255, 255, 90));
        painter.drawEllipse(
            QPointF(dotCenter.x() + (col - 1) * spacing, dotCenter.y() + (row - 1) * spacing * 0.55),
            dotRadius,
            dotRadius);
    }
}

BoxPopup::BoxPopup(StorageBoxApp *app, BoxWindow *boxWindow)
    : QWidget(nullptr), m_app(app), m_boxWindow(boxWindow)
{
    setFixedSize(kCellWidth * 3 + 24, kCellHeight * 3 + 62);
    setWindowIcon(QApplication::windowIcon());
    setAcceptDrops(true);
    applyWindowFlags();
    move(box()->position + QPoint(0, box()->size.height() + 8));
    buildUi();
}

Box *BoxPopup::box() const
{
    return m_boxWindow->box();
}

void BoxPopup::refresh()
{
    applyWindowFlags();
    move(box()->position + QPoint(0, box()->size.height() + 8));
    rebuildGrid();
    update();
    show();
    if (!m_app->alwaysOnTop()) {
        QTimer::singleShot(0, this, [this] { ensureWidgetNonTopmost(this); });
    } else {
        restoreWidgetLayer(this);
    }
}

void BoxPopup::startItemDrag(int index, QWidget *source)
{
    if (index < 0 || index >= box()->items.size()) {
        return;
    }

    const LaunchItem item = box()->items.at(index);
    auto *drag = new QDrag(source);
    auto *mimeData = new QMimeData;
    mimeData->setData(kItemIndexMime, QByteArray::number(index));
    mimeData->setData(kBoxIdMime, box()->id.toUtf8());
    drag->setMimeData(mimeData);

    const QPixmap pixmap = iconForPath(item.path).pixmap(QSize(48, 48));
    if (!pixmap.isNull()) {
        drag->setPixmap(pixmap);
        drag->setHotSpot(QPoint(pixmap.width() / 2, pixmap.height() / 2));
    }

    drag->exec(Qt::MoveAction);
}

bool BoxPopup::handleDropOnSlot(int targetIndex, const QMimeData *mimeData)
{
    if (!mimeData) {
        return false;
    }

    if (mimeData->hasFormat(kItemIndexMime)) {
        if (QString::fromUtf8(mimeData->data(kBoxIdMime)) != box()->id) {
            return false;
        }

        bool ok = false;
        const int sourceIndex = QString::fromUtf8(mimeData->data(kItemIndexMime)).toInt(&ok);
        if (!ok) {
            return false;
        }

        m_app->moveApp(box(), sourceIndex, targetIndex);
        return true;
    }

    const QStringList paths = localFilesFromMime(mimeData);
    if (!paths.isEmpty()) {
        m_app->addDroppedPaths(box(), paths, targetIndex, this);
        return true;
    }

    return false;
}

void BoxPopup::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if ((mimeData && mimeData->hasFormat(kItemIndexMime)) || mimeHasLocalFiles(mimeData)) {
        event->acceptProposedAction();
        return;
    }
    QWidget::dragEnterEvent(event);
}

void BoxPopup::dropEvent(QDropEvent *event)
{
    const int appendIndex = box()->items.size();
    if (handleDropOnSlot(appendIndex, event->mimeData())) {
        event->acceptProposedAction();
        return;
    }
    QWidget::dropEvent(event);
}

void BoxPopup::applyWindowFlags()
{
    if (m_app->alwaysOnTop()) {
        restoreWidgetLayer(this);
    }

    Qt::WindowFlags flags = Qt::FramelessWindowHint | Qt::Tool;
    if (m_app->alwaysOnTop()) {
        flags |= Qt::WindowStaysOnTopHint;
    }
    setWindowFlags(flags);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(
        "BoxPopup { background: #f8fafc; border: 1px solid #cbd5e1; }"
        "QToolButton { border-radius: 7px; border: 1px solid #d1d5db; background: white; color: #111827; padding: 5px; }"
        "QToolButton:hover { background: #dbeafe; border-color: #60a5fa; }"
        "QToolButton#emptySlot { background: #e5e7eb; color: #374151; font-size: 22px; font-weight: 700; }"
        "QToolButton#emptySlot:hover { background: #d1d5db; }"));
}

void BoxPopup::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 8);
    root->setSpacing(8);

    auto *header = new QWidget(this);
    header->setFixedHeight(42);
    header->setStyleSheet(QStringLiteral("background: #111827;"));

    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 5, 6, 5);

    auto *title = new QLabel(box()->name, header);
    title->setStyleSheet(QStringLiteral("color: white; font-weight: 700;"));
    headerLayout->addWidget(title, 1);

    auto *addButton = new QPushButton(QStringLiteral("+"), header);
    addButton->setFixedWidth(34);
    addButton->setStyleSheet(QStringLiteral("background: #1f2937; color: white; border: 0; font-size: 18px; font-weight: 700;"));
    connect(addButton, &QPushButton::clicked, this, [this] { m_app->addApp(box(), this); });
    headerLayout->addWidget(addButton);

    auto *closeButton = new QPushButton(QStringLiteral("x"), header);
    closeButton->setFixedWidth(34);
    closeButton->setStyleSheet(QStringLiteral("background: #1f2937; color: white; border: 0; font-weight: 700;"));
    connect(closeButton, &QPushButton::clicked, m_app, &StorageBoxApp::closePopup);
    headerLayout->addWidget(closeButton);

    root->addWidget(header);

    m_grid = new QGridLayout;
    m_grid->setContentsMargins(8, 0, 8, 0);
    m_grid->setSpacing(6);
    root->addLayout(m_grid);

    for (int index = 0; index < kMaxItemsPerBox; ++index) {
        buildItemButton(index);
    }
}

void BoxPopup::rebuildGrid()
{
    if (!m_grid) {
        return;
    }

    while (QLayoutItem *item = m_grid->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    for (int index = 0; index < kMaxItemsPerBox; ++index) {
        buildItemButton(index);
    }
}

void BoxPopup::buildItemButton(int index)
{
    const int row = index / 3;
    const int col = index % 3;

    const bool occupied = index < box()->items.size();
    auto *button = new SlotButton(this, index, occupied, this);
    button->setFixedSize(kCellWidth, kCellHeight);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setIconSize(QSize(34, 34));

    if (occupied) {
        const LaunchItem item = box()->items.at(index);
        button->setIcon(iconForPath(item.path));
        button->setText(elide(item.name, 14));
        button->setToolTip(item.path);
        connect(button, &QToolButton::clicked, this, [this, item] { m_app->launchApp(item, this); });
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button, &QToolButton::customContextMenuRequested, this, [this, button, index](const QPoint &pos) {
            showItemMenu(button->mapToGlobal(pos), index);
        });
    } else {
        button->setObjectName(QStringLiteral("emptySlot"));
        button->setText(QStringLiteral("+"));
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        connect(button, &QToolButton::clicked, this, [this] { m_app->addApp(box(), this); });
    }

    m_grid->addWidget(button, row, col);
}

void BoxPopup::showItemMenu(const QPoint &globalPos, int index)
{
    QMenu menu(this);
    menu.addAction(QStringLiteral("启动"), this, [this, index] { m_app->launchApp(box()->items.at(index), this); });
    menu.addAction(QStringLiteral("重命名"), this, [this, index] { m_app->renameApp(box(), index, this); });
    menu.addAction(QStringLiteral("移除"), this, [this, index] { m_app->removeApp(box(), index, this); });
    menu.exec(globalPos);
}
