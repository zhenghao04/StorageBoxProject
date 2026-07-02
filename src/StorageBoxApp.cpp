#include "StorageBoxApp.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QColorDialog>
#include <QDateTime>
#include <QDesktopServices>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSize>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
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
constexpr int kConfigSchemaVersion = 2;
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

QIcon colorSwatchIcon(const QColor &color)
{
    QPixmap pixmap(20, 20);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(color);
    painter.setPen(QPen(QColor(15, 23, 42, 50), 1));
    painter.drawRoundedRect(QRectF(2, 2, 16, 16), 5, 5);
    return QIcon(pixmap);
}

const QList<QPair<QString, QColor>> &boxColorThemes()
{
    static const QList<QPair<QString, QColor>> themes = {
        {QStringLiteral("海蓝"), QColor(QStringLiteral("#2563eb"))},
        {QStringLiteral("松绿"), QColor(QStringLiteral("#059669"))},
        {QStringLiteral("琥珀"), QColor(QStringLiteral("#d97706"))},
        {QStringLiteral("紫藤"), QColor(QStringLiteral("#7c3aed"))},
        {QStringLiteral("玫红"), QColor(QStringLiteral("#db2777"))},
        {QStringLiteral("青色"), QColor(QStringLiteral("#0891b2"))},
        {QStringLiteral("石墨"), QColor(QStringLiteral("#475569"))},
    };
    return themes;
}

QPixmap scaledCoverPixmap(const QPixmap &source, const QSize &targetSize)
{
    if (source.isNull() || targetSize.isEmpty()) {
        return {};
    }
    return source.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
}

void drawRoundedPixmap(QPainter &painter, const QPixmap &source, const QRectF &rect, qreal radius)
{
    const QPixmap scaled = scaledCoverPixmap(source, rect.size().toSize());
    if (scaled.isNull()) {
        return;
    }

    QPainterPath clipPath;
    clipPath.addRoundedRect(rect, radius, radius);

    painter.save();
    painter.setClipPath(clipPath);
    const QPointF topLeft(
        rect.center().x() - scaled.width() / 2.0,
        rect.center().y() - scaled.height() / 2.0);
    painter.drawPixmap(topLeft, scaled);
    painter.restore();
}

QPixmap customBoxIconPixmap(const Box *box, const QSize &targetSize)
{
    if (!box || box->iconPath.isEmpty()) {
        return {};
    }

    const QPixmap pixmap(box->iconPath);
    if (pixmap.isNull()) {
        return {};
    }
    return scaledCoverPixmap(pixmap, targetSize);
}

QPixmap boxPreviewPixmap(const Box *box, const QSize &targetSize)
{
    QPixmap pixmap(targetSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF rect(0, 0, targetSize.width(), targetSize.height());
    const qreal radius = qMin(targetSize.width(), targetSize.height()) / 4.0;

    const QPixmap customIcon = customBoxIconPixmap(box, targetSize);
    if (!customIcon.isNull()) {
        drawRoundedPixmap(painter, customIcon, rect, radius);
        painter.setPen(QPen(QColor(255, 255, 255, 190), 1));
        painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
        return pixmap;
    }

    const QColor base = box ? box->color : QColor(QStringLiteral("#2563eb"));
    QLinearGradient gradient(rect.topLeft(), rect.bottomRight());
    gradient.setColorAt(0.0, base.lighter(128));
    gradient.setColorAt(0.58, base);
    gradient.setColorAt(1.0, base.darker(126));
    painter.setBrush(gradient);
    painter.setPen(QPen(QColor(255, 255, 255, 150), 1));
    painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    painter.setPen(Qt::white);
    QFont font(QStringLiteral("Microsoft YaHei UI"), qMax(10, targetSize.height() / 3), QFont::DemiBold);
    painter.setFont(font);
    painter.drawText(rect, Qt::AlignCenter, box && !box->name.isEmpty() ? box->name.left(1).toUpper() : QStringLiteral("S"));
    return pixmap;
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
    m_boxes.clear();
    m_alwaysOnTop = false;

    QFile file(configFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        ensureDefaults();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        backupConfigFile(QStringLiteral("invalid"));
        QFile::remove(configFilePath());
        QMessageBox::warning(
            nullptr,
            QStringLiteral("配置已恢复"),
            QStringLiteral("配置文件无法读取，已保留备份并使用默认配置。"));
        ensureDefaults();
        return;
    }

    const QJsonObject root = document.object();
    const int schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(1);
    if (schemaVersion > kConfigSchemaVersion) {
        QMessageBox::warning(
            nullptr,
            QStringLiteral("配置版本较新"),
            QStringLiteral("当前程序版本较旧，会尽量读取现有配置。建议先备份配置文件。"));
    }

    m_alwaysOnTop = root.value(QStringLiteral("alwaysOnTop")).toBool(false);

    const QJsonArray boxes = root.value(QStringLiteral("boxes")).toArray();
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
        QString(),
        {},
    });
}

void StorageBoxApp::save()
{
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), kConfigSchemaVersion);
    root.insert(QStringLiteral("savedAtUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert(QStringLiteral("alwaysOnTop"), m_alwaysOnTop);

    QJsonArray boxes;
    for (const Box &box : m_boxes) {
        boxes.append(boxToJson(box));
    }
    root.insert(QStringLiteral("boxes"), boxes);

    QFileInfo info(configFilePath());
    QDir().mkpath(info.absolutePath());

    backupConfigFile(QStringLiteral("latest"));

    QSaveFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
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

    QAction *startup = menu->addAction(QStringLiteral("开机自启动"));
    startup->setCheckable(true);
    startup->setChecked(startAtLogin());
    connect(startup, &QAction::toggled, this, [this, startup](bool enabled) {
        if (!setStartAtLogin(enabled, nullptr)) {
            const bool wasBlocked = startup->blockSignals(true);
            startup->setChecked(startAtLogin());
            startup->blockSignals(wasBlocked);
        }
    });

    menu->addAction(QStringLiteral("打开配置文件夹"), this, [this] {
        openConfigFolder(nullptr);
    });

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

QString StorageBoxApp::configBackupFilePath() const
{
    const QFileInfo configInfo(configFilePath());
    return QDir(configInfo.absolutePath()).filePath(QStringLiteral("config.backup.json"));
}

QString StorageBoxApp::iconStorageDirPath() const
{
    const QFileInfo configInfo(configFilePath());
    return QDir(configInfo.absolutePath()).filePath(QStringLiteral("Icons"));
}

QString StorageBoxApp::startupShortcutPath() const
{
#ifdef Q_OS_WIN
    const QString appData = qEnvironmentVariable("APPDATA");
    if (appData.isEmpty()) {
        return {};
    }
    return QDir(appData).filePath(QStringLiteral("Microsoft/Windows/Start Menu/Programs/Startup/Storage Box Launcher.lnk"));
#else
    return {};
#endif
}

void StorageBoxApp::backupConfigFile(const QString &label) const
{
    const QFileInfo configInfo(configFilePath());
    if (!configInfo.exists() || configInfo.size() <= 0) {
        return;
    }

    QDir().mkpath(configInfo.absolutePath());
    QString targetPath = configBackupFilePath();
    if (label != QStringLiteral("latest")) {
        const QString safeLabel = label.simplified().replace(QLatin1Char(' '), QLatin1Char('-'));
        const QString timestamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        targetPath = QDir(configInfo.absolutePath()).filePath(
            QStringLiteral("config.%1.%2.json").arg(safeLabel.isEmpty() ? QStringLiteral("backup") : safeLabel, timestamp));
    }

    QFile::remove(targetPath);
    QFile::copy(configInfo.absoluteFilePath(), targetPath);
}

QString StorageBoxApp::copyIconToStorage(const QString &sourcePath, const QString &boxId) const
{
    const QImage image(sourcePath);
    if (image.isNull()) {
        return {};
    }

    const QFileInfo sourceInfo(sourcePath);
    QString suffix = sourceInfo.suffix().toLower();
    if (suffix.isEmpty() || suffix.size() > 8) {
        suffix = QStringLiteral("png");
    }

    QDir iconDir(iconStorageDirPath());
    if (!iconDir.exists() && !QDir().mkpath(iconDir.absolutePath())) {
        return {};
    }

    const QString safeId = boxId.isEmpty() ? makeId() : boxId;
    const QString targetPath = iconDir.filePath(QStringLiteral("%1.%2").arg(safeId, suffix));
    const QString sourceAbs = QFileInfo(sourcePath).absoluteFilePath();
    const QString targetAbs = QFileInfo(targetPath).absoluteFilePath();
    if (QString::compare(sourceAbs, targetAbs, Qt::CaseInsensitive) == 0) {
        return QDir::toNativeSeparators(targetPath);
    }

    QFile::remove(targetPath);
    if (QFile::copy(sourcePath, targetPath)) {
        return QDir::toNativeSeparators(targetPath);
    }

    const QString pngTargetPath = iconDir.filePath(QStringLiteral("%1.png").arg(safeId));
    QFile::remove(pngTargetPath);
    if (image.save(pngTargetPath, "PNG")) {
        return QDir::toNativeSeparators(pngTargetPath);
    }

    return {};
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
        QString(),
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

void StorageBoxApp::setBoxColor(Box *box, const QColor &color)
{
    if (!box || !color.isValid()) {
        return;
    }

    box->color = color;
    refreshViews(box);
}

void StorageBoxApp::chooseBoxColor(Box *box, QWidget *parent)
{
    if (!box) {
        return;
    }

    const QColor color = QColorDialog::getColor(
        box->color,
        parent,
        QStringLiteral("选择盒子颜色"));
    setBoxColor(box, color);
}

void StorageBoxApp::chooseBoxIcon(Box *box, QWidget *parent)
{
    if (!box) {
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        parent,
        QStringLiteral("选择盒子图标图片"),
        QString(),
        QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.ico *.webp);;所有文件 (*.*)"));
    if (path.isEmpty()) {
        return;
    }

    const QString storedPath = copyIconToStorage(path, box->id);
    if (storedPath.isEmpty()) {
        QMessageBox::warning(parent, QStringLiteral("图标不可用"), QStringLiteral("无法读取或保存这张图片。"));
        return;
    }

    box->iconPath = storedPath;
    refreshViews(box);
}

void StorageBoxApp::clearBoxIcon(Box *box)
{
    if (!box || box->iconPath.isEmpty()) {
        return;
    }

    box->iconPath.clear();
    refreshViews(box);
}

void StorageBoxApp::addApp(Box *box, QWidget *parent)
{
    if (box->items.size() >= kMaxItemsPerBox) {
        QMessageBox::information(parent, QStringLiteral("盒子已满"), QStringLiteral("每个盒子最多收纳 9 个项目。"));
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        parent,
        QStringLiteral("选择应用、文件或快捷方式"),
        QString(),
        QStringLiteral("常用项目 (*.exe *.lnk *.url *.pdf *.doc *.docx *.xls *.xlsx *.ppt *.pptx *.txt *.md *.png *.jpg *.jpeg *.zip *.rar);;所有文件 (*.*)"));
    if (path.isEmpty()) {
        return;
    }

    bool ok = false;
    const QString defaultName = defaultNameForPath(path);
    QString name = QInputDialog::getText(
        parent,
        QStringLiteral("显示名称"),
        QStringLiteral("显示名称："),
        QLineEdit::Normal,
        defaultName,
        &ok);
    if (!ok || name.trimmed().isEmpty()) {
        name = defaultName;
    }

    box->items.append(LaunchItem{name.trimmed(), path});
    renderBoxes();
}

void StorageBoxApp::addFolder(Box *box, QWidget *parent)
{
    if (box->items.size() >= kMaxItemsPerBox) {
        QMessageBox::information(parent, QStringLiteral("盒子已满"), QStringLiteral("每个盒子最多收纳 9 个项目。"));
        return;
    }

    const QString path = QFileDialog::getExistingDirectory(
        parent,
        QStringLiteral("选择文件夹"),
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (path.isEmpty()) {
        return;
    }

    bool ok = false;
    const QString defaultName = defaultNameForPath(path);
    QString name = QInputDialog::getText(
        parent,
        QStringLiteral("显示名称"),
        QStringLiteral("显示名称："),
        QLineEdit::Normal,
        defaultName,
        &ok);
    if (!ok || name.trimmed().isEmpty()) {
        name = defaultName;
    }

    box->items.append(LaunchItem{name.trimmed(), QDir::toNativeSeparators(path)});
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
        QStringLiteral("重命名项目"),
        QStringLiteral("显示名称："),
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
    if (QMessageBox::question(parent, QStringLiteral("移除项目"), QStringLiteral("从盒子里移除「%1」吗？").arg(name)) != QMessageBox::Yes) {
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
        QMessageBox::information(parent, QStringLiteral("盒子已满"), QStringLiteral("每个盒子最多收纳 9 个项目。"));
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
        QMessageBox::warning(parent, QStringLiteral("无法打开"), QStringLiteral("找不到项目：\n%1").arg(item.path));
        return;
    }

    closePopup();

#ifdef Q_OS_WIN
    const std::wstring path = QDir::toNativeSeparators(item.path).toStdWString();
    const HINSTANCE result = ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        QMessageBox::warning(parent, QStringLiteral("打开失败"), QStringLiteral("Windows 无法打开：\n%1").arg(item.path));
    }
#else
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(item.path))) {
        QMessageBox::warning(parent, QStringLiteral("打开失败"), QStringLiteral("无法打开：\n%1").arg(item.path));
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

void StorageBoxApp::openConfigFolder(QWidget *parent) const
{
    const QFileInfo configInfo(configFilePath());
    QDir().mkpath(configInfo.absolutePath());
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(configInfo.absolutePath()))) {
        QMessageBox::warning(parent, QStringLiteral("打开失败"), QStringLiteral("无法打开配置文件夹：\n%1").arg(configInfo.absolutePath()));
    }
}

bool StorageBoxApp::startAtLogin() const
{
    const QString shortcutPath = startupShortcutPath();
    return !shortcutPath.isEmpty() && QFileInfo::exists(shortcutPath);
}

bool StorageBoxApp::setStartAtLogin(bool enabled, QWidget *parent)
{
#ifdef Q_OS_WIN
    const QString shortcutPath = startupShortcutPath();
    if (shortcutPath.isEmpty()) {
        QMessageBox::warning(parent, QStringLiteral("设置失败"), QStringLiteral("无法定位 Windows 启动目录。"));
        return false;
    }

    if (enabled) {
        const QFileInfo shortcutInfo(shortcutPath);
        QDir().mkpath(shortcutInfo.absolutePath());
        QFile::remove(shortcutPath);
        if (!QFile::link(QApplication::applicationFilePath(), shortcutPath)) {
            QMessageBox::warning(parent, QStringLiteral("设置失败"), QStringLiteral("无法创建开机启动快捷方式：\n%1").arg(shortcutPath));
            return false;
        }
        return true;
    }

    if (QFileInfo::exists(shortcutPath) && !QFile::remove(shortcutPath)) {
        QMessageBox::warning(parent, QStringLiteral("设置失败"), QStringLiteral("无法删除开机启动快捷方式：\n%1").arg(shortcutPath));
        return false;
    }
    return true;
#else
    Q_UNUSED(enabled);
    QMessageBox::information(parent, QStringLiteral("暂不支持"), QStringLiteral("开机自启动设置目前只支持 Windows。"));
    return false;
#endif
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
    const auto &themes = boxColorThemes();
    return themes.at(index % themes.size()).second;
}

QString StorageBoxApp::defaultNameForPath(const QString &path) const
{
    const QFileInfo info(path);
    if (info.isDir()) {
        return info.fileName().isEmpty() ? path : info.fileName();
    }
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
    object.insert(QStringLiteral("iconPath"), box.iconPath);

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
    if (!box.color.isValid()) {
        box.color = colorForIndex(index);
    }
    box.iconPath = json.value(QStringLiteral("iconPath")).toString();

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
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("左键打开，拖动移动，拖边缩放，右键管理，双击添加文件/应用；可拖入文件、文件夹或文档"));
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

bool BoxWindow::event(QEvent *event)
{
    if (event->type() == QEvent::Enter || event->type() == QEvent::HoverEnter) {
        m_hovered = true;
        update();
    } else if (event->type() == QEvent::Leave || event->type() == QEvent::HoverLeave) {
        m_hovered = false;
        update();
    }
    return QWidget::event(event);
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
    QMenu *addMenu = menu.addMenu(QStringLiteral("添加项目"));
    addMenu->addAction(QStringLiteral("添加文件/应用..."), this, [this] { m_app->addApp(m_box, this); });
    addMenu->addAction(QStringLiteral("添加文件夹..."), this, [this] { m_app->addFolder(m_box, this); });
    menu.addAction(QStringLiteral("重命名盒子"), this, [this] { m_app->renameBox(m_box, this); });

    QMenu *appearanceMenu = menu.addMenu(QStringLiteral("外观"));
    QMenu *colorMenu = appearanceMenu->addMenu(QStringLiteral("颜色主题"));
    for (const auto &theme : boxColorThemes()) {
        QAction *action = colorMenu->addAction(colorSwatchIcon(theme.second), theme.first);
        action->setCheckable(true);
        action->setChecked(QString::compare(m_box->color.name(), theme.second.name(), Qt::CaseInsensitive) == 0);
        connect(action, &QAction::triggered, this, [this, color = theme.second] {
            m_app->setBoxColor(m_box, color);
        });
    }
    colorMenu->addSeparator();
    colorMenu->addAction(QStringLiteral("自定义颜色..."), this, [this] {
        m_app->chooseBoxColor(m_box, this);
    });

    appearanceMenu->addSeparator();
    appearanceMenu->addAction(QStringLiteral("选择图片图标..."), this, [this] {
        m_app->chooseBoxIcon(m_box, this);
    });
    QAction *clearIcon = appearanceMenu->addAction(QStringLiteral("清除图片图标"), this, [this] {
        m_app->clearBoxIcon(m_box);
    });
    clearIcon->setEnabled(!m_box->iconPath.isEmpty());
    menu.addSeparator();

    QAction *topmost = menu.addAction(QStringLiteral("置顶显示"));
    topmost->setCheckable(true);
    topmost->setChecked(m_app->alwaysOnTop());
    connect(topmost, &QAction::toggled, m_app, &StorageBoxApp::setAlwaysOnTop);

    QAction *startup = menu.addAction(QStringLiteral("开机自启动"));
    startup->setCheckable(true);
    startup->setChecked(m_app->startAtLogin());
    connect(startup, &QAction::toggled, this, [this, startup](bool enabled) {
        if (!m_app->setStartAtLogin(enabled, this)) {
            const bool wasBlocked = startup->blockSignals(true);
            startup->setChecked(m_app->startAtLogin());
            startup->blockSignals(wasBlocked);
        }
    });

    menu.addAction(QStringLiteral("打开配置文件夹"), this, [this] {
        m_app->openConfigFolder(this);
    });

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
        m_dropActive = true;
        update();
        event->acceptProposedAction();
        return;
    }
    QWidget::dragEnterEvent(event);
}

void BoxWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
    m_dropActive = false;
    update();
    QWidget::dragLeaveEvent(event);
}

void BoxWindow::dropEvent(QDropEvent *event)
{
    m_dropActive = false;
    update();
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
    const qreal radius = qBound(12.0, side / 4.3, 26.0);

    const QRectF shadowRect(5, 7, width() - 10, height() - 12);
    QPainterPath shadowPath;
    shadowPath.addRoundedRect(shadowRect, radius + 2, radius + 2);
    painter.fillPath(shadowPath, QColor(15, 23, 42, m_hovered ? 70 : 44));

    const QRectF outer(4, 3, width() - 8, height() - 10);
    QPainterPath path;
    path.addRoundedRect(outer, radius, radius);

    const QColor base = m_hovered ? m_box->color.lighter(112) : m_box->color;
    QLinearGradient background(outer.topLeft(), outer.bottomRight());
    background.setColorAt(0.0, base.lighter(132));
    background.setColorAt(0.5, base);
    background.setColorAt(1.0, base.darker(126));
    painter.fillPath(path, background);

    QLinearGradient sheen(outer.topLeft(), QPointF(outer.left(), outer.center().y()));
    sheen.setColorAt(0.0, QColor(255, 255, 255, m_hovered ? 76 : 54));
    sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.save();
    painter.setClipPath(path);
    painter.fillRect(outer.adjusted(1, 1, -1, -outer.height() * 0.45), sheen);
    painter.restore();

    painter.setPen(QPen(QColor(255, 255, 255, (m_hovered || m_dropActive) ? 230 : 178), (m_hovered || m_dropActive) ? 2.0 : 1.4));
    painter.drawPath(path);

    if (m_dropActive) {
        QPen dropPen(QColor(255, 255, 255, 245), 2.2, Qt::DashLine);
        painter.setPen(dropPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(outer.adjusted(4, 4, -4, -4), qMax(8.0, radius - 4), qMax(8.0, radius - 4));
    }

    const QString countText = QStringLiteral("%1/9").arg(m_box->items.size());
    QFont countFont(QStringLiteral("Segoe UI"), qBound(6, side / 11, 9), QFont::DemiBold);
    painter.setFont(countFont);
    const QFontMetrics countMetrics(countFont);
    const int pillHeight = qBound(15, side / 5, 22);
    const int pillWidth = qMax(countMetrics.horizontalAdvance(countText) + 12, pillHeight + 8);
    const QRectF pillRect(outer.right() - pillWidth - 7, outer.top() + 7, pillWidth, pillHeight);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(15, 23, 42, 54));
    painter.drawRoundedRect(pillRect, pillHeight / 2.0, pillHeight / 2.0);
    painter.setPen(QColor(255, 255, 255, 230));
    painter.drawText(pillRect, Qt::AlignCenter, countText);

    const QPixmap customIcon = customBoxIconPixmap(m_box, QSize(qMax(1, width()), qMax(1, height())));
    if (!customIcon.isNull()) {
        const qreal iconSide = qBound(
            24.0,
            qMin(width() * 0.48, height() * 0.44),
            72.0);
        const QRectF iconRect(
            (width() - iconSide) / 2.0,
            qMax(18.0, height() * 0.47 - iconSide / 2.0),
            iconSide,
            iconSide);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 60));
        painter.drawRoundedRect(iconRect.adjusted(-4, -4, 4, 4), iconSide / 4.0 + 4, iconSide / 4.0 + 4);
        drawRoundedPixmap(painter, customIcon, iconRect, iconSide / 4.0);
    } else {
        const qreal gridSide = qBound(
            25.0,
            qMin(width() * 0.43, height() * 0.38),
            60.0);
        const QRectF gridRect(
            (width() - gridSide) / 2.0,
            qMax(21.0, height() * 0.48 - gridSide / 2.0),
            gridSide,
            gridSide);
        const qreal gap = qMax(2.0, gridSide / 12.0);
        const qreal cell = (gridSide - gap * 2) / 3.0;
        painter.setPen(Qt::NoPen);
        for (int i = 0; i < kMaxItemsPerBox; ++i) {
            const int row = i / 3;
            const int col = i % 3;
            const QRectF slot(
                gridRect.left() + col * (cell + gap),
                gridRect.top() + row * (cell + gap),
                cell,
                cell);
            painter.setBrush(i < m_box->items.size() ? QColor(255, 255, 255, 230) : QColor(255, 255, 255, 86));
            painter.drawRoundedRect(slot, qMax(2.0, cell / 4.0), qMax(2.0, cell / 4.0));
        }
    }

    QFont nameFont(QStringLiteral("Microsoft YaHei UI"), qBound(7, side / 9, 11), QFont::DemiBold);
    painter.setFont(nameFont);
    painter.setPen(Qt::white);
    const int titleHeight = qBound(17, side / 4, 26);
    const QRect titleRect(9, height() - titleHeight - 9, width() - 18, titleHeight);
    const QString titleText = QFontMetrics(nameFont).elidedText(m_box->name, Qt::ElideRight, titleRect.width());
    painter.drawText(titleRect, Qt::AlignCenter, titleText);

    if (m_hovered) {
        painter.setPen(QPen(QColor(255, 255, 255, 125), 1));
        const QPointF corner(outer.right() - 8, outer.bottom() - 8);
        painter.drawLine(corner + QPointF(-8, 8), corner + QPointF(8, -8));
        painter.drawLine(corner + QPointF(-3, 8), corner + QPointF(8, -3));
    }
}

BoxPopup::BoxPopup(StorageBoxApp *app, BoxWindow *boxWindow)
    : QWidget(nullptr), m_app(app), m_boxWindow(boxWindow)
{
    setFixedSize(kCellWidth * 3 + 48, kCellHeight * 3 + 96);
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
    updateHeader();
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
    setAttribute(Qt::WA_TranslucentBackground, true);
    setStyleSheet(QStringLiteral(
        "BoxPopup { background: transparent; }"
        "QFrame#popupPanel { background: rgba(248, 250, 252, 242); border: 1px solid rgba(148, 163, 184, 150); border-radius: 18px; }"
        "QLabel#popupTitle { color: #0f172a; font-size: 14px; font-weight: 700; }"
        "QLabel#popupCount { color: #64748b; font-size: 11px; }"
        "QPushButton#headerButton { min-width: 30px; max-width: 30px; min-height: 30px; max-height: 30px; border: 0; border-radius: 15px; background: #e2e8f0; color: #0f172a; font-size: 16px; font-weight: 700; }"
        "QPushButton#headerButton:hover { background: #cbd5e1; }"
        "QToolButton { border-radius: 12px; border: 1px solid rgba(203, 213, 225, 210); background: rgba(255, 255, 255, 235); color: #111827; padding: 5px; }"
        "QToolButton:hover { background: #eff6ff; border-color: #60a5fa; }"
        "QToolButton:pressed { background: #dbeafe; }"
        "QToolButton#emptySlot { border: 1px dashed #94a3b8; background: rgba(241, 245, 249, 205); color: #64748b; font-size: 24px; font-weight: 600; }"
        "QToolButton#emptySlot:hover { background: #e2e8f0; border-color: #64748b; color: #334155; }"));
}

void BoxPopup::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 14);
    root->setSpacing(0);

    auto *panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("popupPanel"));
    auto *shadow = new QGraphicsDropShadowEffect(panel);
    shadow->setBlurRadius(28);
    shadow->setColor(QColor(15, 23, 42, 72));
    shadow->setOffset(0, 9);
    panel->setGraphicsEffect(shadow);
    root->addWidget(panel);

    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(12, 10, 12, 12);
    panelLayout->setSpacing(10);

    auto *header = new QWidget(panel);
    header->setFixedHeight(42);

    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);

    m_iconLabel = new QLabel(header);
    m_iconLabel->setFixedSize(34, 34);
    m_iconLabel->setScaledContents(true);
    headerLayout->addWidget(m_iconLabel);

    auto *titleStack = new QWidget(header);
    auto *titleLayout = new QVBoxLayout(titleStack);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(1);

    m_titleLabel = new QLabel(header);
    m_titleLabel->setObjectName(QStringLiteral("popupTitle"));
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    titleLayout->addWidget(m_titleLabel);

    m_countLabel = new QLabel(header);
    m_countLabel->setObjectName(QStringLiteral("popupCount"));
    titleLayout->addWidget(m_countLabel);
    headerLayout->addWidget(titleStack, 1);

    auto *addButton = new QPushButton(QStringLiteral("+"), header);
    addButton->setObjectName(QStringLiteral("headerButton"));
    addButton->setToolTip(QStringLiteral("添加文件、应用或文件夹"));
    connect(addButton, &QPushButton::clicked, this, [this, addButton] { showAddMenu(addButton); });
    headerLayout->addWidget(addButton);

    auto *closeButton = new QPushButton(QStringLiteral("x"), header);
    closeButton->setObjectName(QStringLiteral("headerButton"));
    closeButton->setToolTip(QStringLiteral("关闭"));
    connect(closeButton, &QPushButton::clicked, m_app, &StorageBoxApp::closePopup);
    headerLayout->addWidget(closeButton);

    panelLayout->addWidget(header);

    m_grid = new QGridLayout;
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setSpacing(7);
    panelLayout->addLayout(m_grid);

    updateHeader();

    for (int index = 0; index < kMaxItemsPerBox; ++index) {
        buildItemButton(index);
    }
}

void BoxPopup::updateHeader()
{
    if (m_iconLabel) {
        m_iconLabel->setPixmap(boxPreviewPixmap(box(), QSize(34, 34)));
    }

    if (m_titleLabel) {
        QFontMetrics metrics(m_titleLabel->font());
        m_titleLabel->setText(metrics.elidedText(box()->name, Qt::ElideRight, 220));
    }

    if (m_countLabel) {
        m_countLabel->setText(QStringLiteral("%1/9").arg(box()->items.size()));
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
    button->setCursor(Qt::PointingHandCursor);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setIconSize(QSize(36, 36));

    if (occupied) {
        const LaunchItem item = box()->items.at(index);
        const bool pathExists = QFileInfo::exists(item.path);
        button->setIcon(iconForPath(item.path));
        button->setText(elide(item.name, 14));
        button->setToolTip(pathExists
            ? item.path
            : QStringLiteral("%1\n项目不存在，可能已被移动或删除。").arg(item.path));
        connect(button, &QToolButton::clicked, this, [this, item] { m_app->launchApp(item, this); });
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button, &QToolButton::customContextMenuRequested, this, [this, button, index](const QPoint &pos) {
            showItemMenu(button->mapToGlobal(pos), index);
        });
    } else {
        button->setObjectName(QStringLiteral("emptySlot"));
        button->setText(QStringLiteral("+"));
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setToolTip(QStringLiteral("点击添加，或把桌面文件、文件夹、文档拖到这里"));
        button->style()->unpolish(button);
        button->style()->polish(button);
        connect(button, &QToolButton::clicked, this, [this, button] { showAddMenu(button); });
    }

    m_grid->addWidget(button, row, col);
}

void BoxPopup::showAddMenu(QWidget *anchor)
{
    if (!anchor) {
        return;
    }

    QMenu menu(this);
    menu.addAction(QStringLiteral("添加文件/应用..."), this, [this] { m_app->addApp(box(), this); });
    menu.addAction(QStringLiteral("添加文件夹..."), this, [this] { m_app->addFolder(box(), this); });
    menu.exec(anchor->mapToGlobal(QPoint(0, anchor->height() + 4)));
}

void BoxPopup::showItemMenu(const QPoint &globalPos, int index)
{
    QMenu menu(this);
    menu.addAction(QStringLiteral("打开"), this, [this, index] { m_app->launchApp(box()->items.at(index), this); });
    menu.addAction(QStringLiteral("重命名"), this, [this, index] { m_app->renameApp(box(), index, this); });
    menu.addAction(QStringLiteral("移除"), this, [this, index] { m_app->removeApp(box(), index, this); });
    menu.exec(globalPos);
}
