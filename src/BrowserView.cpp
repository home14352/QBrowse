#include "BrowserView.h"
#include "MainWindow.h"
#include <QDialog>
#include <QLabel>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWebEngineDesktopMediaRequest>
#include <QWebEnginePage>
#include <QWebEnginePermission>
#include <QWebEngineProfile>

namespace {
QString permissionName(QWebEnginePermission::PermissionType type) {
    using T = QWebEnginePermission::PermissionType;
    switch (type) {
    case T::MediaAudioCapture: return QStringLiteral("микрофон");
    case T::MediaVideoCapture: return QStringLiteral("камера");
    case T::MediaAudioVideoCapture: return QStringLiteral("камера и микрофон");
    case T::Notifications: return QStringLiteral("уведомления");
    case T::Geolocation: return QStringLiteral("геолокация");
    case T::ClipboardReadWrite: return QStringLiteral("буфер обмена");
    case T::LocalFontsAccess: return QStringLiteral("локальные шрифты");
    default: return QStringLiteral("доступ");
    }
}
}

BrowserView::BrowserView(QWebEngineProfile *profile, MainWindow *window, QWidget *parent)
    : QWebEngineView(parent)
    , m_window(window) {
    setPage(new QWebEnginePage(profile, this));
    setupPermissionPrompts();
}

void BrowserView::setupPermissionPrompts() {
    connect(page(), &QWebEnginePage::permissionRequested, this, [this](QWebEnginePermission permission) {
        const QString question = QStringLiteral("Сайт %1 запрашивает %2. Разрешить?")
            .arg(permission.origin().host(), permissionName(permission.permissionType()));
        auto answer = QMessageBox::question(this,
                                            QStringLiteral("Разрешение сайта"),
                                            question,
                                            QMessageBox::Yes | QMessageBox::No,
                                            QMessageBox::No);
        if (answer == QMessageBox::Yes) permission.grant(); else permission.deny();
    });
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    connect(page(), &QWebEnginePage::desktopMediaRequested, this, [this](QWebEngineDesktopMediaRequest request) {
        QDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("Выбор экрана или окна"));
        auto *layout = new QVBoxLayout(&dlg);
        auto *label = new QLabel(QStringLiteral("%1 хочет захватить экран. Выбери источник:").arg(page()->url().host()));
        layout->addWidget(label);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
        auto *screenBtn = new QPushButton(QStringLiteral("Весь экран"));
        auto *windowBtn = new QPushButton(QStringLiteral("Окно"));
        buttons->addButton(screenBtn, QDialogButtonBox::AcceptRole);
        buttons->addButton(windowBtn, QDialogButtonBox::AcceptRole);
        layout->addWidget(buttons);
        QObject::connect(screenBtn, &QPushButton::clicked, &dlg, [&]() {
            const auto idx = request.screensModel()->index(0, 0);
            if (idx.isValid()) request.selectScreen(idx); else request.cancel();
            dlg.accept();
        });
        QObject::connect(windowBtn, &QPushButton::clicked, &dlg, [&]() {
            const auto idx = request.windowsModel()->index(0, 0);
            if (idx.isValid()) request.selectWindow(idx); else request.cancel();
            dlg.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, [&]() { request.cancel(); dlg.reject(); });
        dlg.exec();
    });
#endif
}

QWebEngineView *BrowserView::createWindow(QWebEnginePage::WebWindowType type) {
    Q_UNUSED(type);
    if (!m_window) return nullptr;
    return m_window->createDetachedView();
}
