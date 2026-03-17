#pragma once
#include <QWebEngineView>

class QWebEngineProfile;
class MainWindow;

class BrowserView : public QWebEngineView {
    Q_OBJECT
public:
    explicit BrowserView(QWebEngineProfile *profile, MainWindow *window, QWidget *parent = nullptr);
protected:
    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;
private:
    MainWindow *m_window;
    void setupPermissionPrompts();
};
