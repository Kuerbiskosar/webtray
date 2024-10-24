#include <QtCore/QProcess>
#include <QtGui/QCloseEvent>
#include <QtGui/QDesktopServices>
#include <QtWebEngineCore/QWebEngineCookieStore>
#include <QtWebEngineCore/QWebEngineFullScreenRequest>
#include <QtWebEngineCore/QWebEngineNewWindowRequest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFileDialog>
#include <filesystem>

#include "QtWebEngineCore/qwebenginedownloadrequest.h"
#include "permissionmanager.hpp"
#include "webwindow.hpp"

WebWindow::WebWindow(const QString &url)
	: QMainWindow()
{
	this->profile = new QWebEngineProfile(QUrl(url).host());

	this->page = new QWebEnginePage(this->profile);
	this->perm = new PermissionManager(
		(profile->persistentStoragePath() + "/permissions.state").toStdString());
	this->view = new QWebEngineView(this->page);

	this->web_configure();

	this->profile->setPersistentCookiesPolicy(
		QWebEngineProfile::AllowPersistentCookies);

	this->view->setUrl(url);

	this->setCentralWidget(this->view);
}

void
WebWindow::web_configure()
{
	this->page->settings()->setAttribute(QWebEngineSettings::ScreenCaptureEnabled,
	                                     true);

	this->page->settings()->setAttribute(
		QWebEngineSettings::WebRTCPublicInterfacesOnly, false);

	this->page->settings()->setAttribute(
		QWebEngineSettings::ScrollAnimatorEnabled, false);
	this->page->settings()->setAttribute(
		QWebEngineSettings::FullScreenSupportEnabled, true);

	this->profile->setPushServiceEnabled(true);

	this->page->connect(this->page,
	                    &QWebEnginePage::featurePermissionRequested,
	                    [&](const QUrl origin, QWebEnginePage::Feature feature) {
												this->permission_requested(origin, feature);
											});

	this->profile->connect(this->profile,
	                       &QWebEngineProfile::downloadRequested,
	                       [&](QWebEngineDownloadRequest *request) {
													 const QUrl url = QFileDialog::getSaveFileUrl(
														 this,
														 "",
														 QUrl(this->profile->downloadPath() + "/" +
		                              request->downloadFileName()));
													 if (!url.isEmpty()) {
														 request->setDownloadFileName(url.path());
														 request->accept();
													 }
												 });

	this->page->connect(this->page,
	                    &QWebEnginePage::newWindowRequested,
	                    [=](QWebEngineNewWindowRequest &request) {
												QDesktopServices::openUrl(request.requestedUrl());
											});

	this->page->connect(this->page,
	                    &QWebEnginePage::fullScreenRequested,
	                    [=](QWebEngineFullScreenRequest request) {
												request.accept();
												if (request.toggleOn()) {
													this->showFullScreen();
												} else {
													this->showNormal();
												}
											});
}

void
WebWindow::permission_requested(const QUrl origin,
                                QWebEnginePage::Feature feature)
{
	if (this->perm->get(feature)) {
		this->page->setFeaturePermission(
			origin, feature, QWebEnginePage::PermissionGrantedByUser);
	} else {
		this->page->setFeaturePermission(
			origin, feature, QWebEnginePage::PermissionDeniedByUser);
	}
	this->page->setFeaturePermission(
		origin, feature, QWebEnginePage::PermissionUnknown);
}

void
WebWindow::closeEvent(QCloseEvent *event)
{
	this->hide();
	event->ignore();
}

void
WebWindow::connect_icon_changed(std::function<void(const QIcon)> fn)
{
	this->view->connect(this->view, &QWebEngineView::iconChanged, fn);
}

void
WebWindow::connect_notification(
	std::function<void(std::unique_ptr<QWebEngineNotification>)> fn)
{
	this->profile->setNotificationPresenter(fn);
}

void
WebWindow::connect_title_changed(std::function<void(const QString)> fn)
{
	this->view->connect(this->view, &QWebEngineView::titleChanged, fn);
}

PermissionManager &
WebWindow::permissions()
{
	return *this->perm;
}

void
WebWindow::reset_cookies()
{
	std::filesystem::remove_all(
		this->profile->persistentStoragePath().toStdString());
	QProcess process;
	QStringList args = QApplication::instance()->arguments();

	if (!args.contains("--open-at-startup")) {
		args.append("--open-at-startup");
	}

	process.startDetached(args.front(), args);
	this->quit();
}

void
WebWindow::toggle_visibility()
{
	this->setVisible(!this->isVisible());
}

void
WebWindow::quit()
{
	this->page->disconnect();
	this->view->disconnect();
	this->profile->disconnect();
	delete this->view;
	delete this->page;
	delete this->perm;
	QApplication::instance()->quit();
}
