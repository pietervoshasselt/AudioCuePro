#include <QApplication>
#include <QStyleFactory>
#include <QStandardPaths>
#include <QDir>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QString style = R"(
        QWidget {
            background-color: #1e1e1e;
            color: #dddddd;
            font-size: 14px;
        }

        QPushButton {
            background-color: #2e2e2e;
            border: 1px solid #444;
            padding: 6px 10px;
            border-radius: 6px;
        }
        QPushButton:hover {
            background-color: #3c3c3c;
            border-color: #66aaff;
        }
        QPushButton:pressed {
            background-color: #2a2a2a;
        }

        QLineEdit, QSpinBox, QDoubleSpinBox, QTextEdit {
            background-color: #2b2b2b;
            border: 1px solid #555;
            border-radius: 4px;
            color: #e0e0e0;
        }

        QComboBox {
            background-color: #2b2b2b;
            border: 1px solid #555;
            padding: 3px;
            border-radius: 4px;
        }

        QScrollArea {
            background-color: #1e1e1e;
            border: none;
        }

        /* Track card styling */
        QWidget#trackCard {
            background-color: #252526;
            border: 1px solid #3a3a3a;
            border-radius: 10px;
        }
        QWidget#trackCard:hover {
            border-color: #5e9cff;
        }

        QLabel#trackName {
            font-weight: 600;
            font-size: 15px;
        }

        QLabel#trackStatus {
            font-size: 18px;
        }

        /* Play/Pause/Stop button colors */
        QPushButton#playButton {
            background-color: #155724;
            border-color: #1c7c35;
        }
        QPushButton#playButton:hover {
            background-color: #1f7a31;
            border-color: #28a745;
        }

        QPushButton#pauseButton {
            background-color: #856404;
            border-color: #b38600;
        }
        QPushButton#pauseButton:hover {
            background-color: #a87b06;
            border-color: #e0a800;
        }

        QPushButton#stopButton {
            background-color: #721c24;
            border-color: #b21f2d;
        }
        QPushButton#stopButton:hover {
            background-color: #a12632;
            border-color: #dc3545;
        }

        /* Scrollbar styling */
        QScrollBar:vertical {
            background: #1e1e1e;
            width: 10px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #3a3a3a;
            min-height: 20px;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical:hover {
            background: #5a5a5a;
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0;
        }

        /* Empty state / welcome panel – full screen feeling */
        QWidget#emptyState {
            background-color: #1e1e1e;
        }
        QLabel#emptyStateTitle {
            font-size: 28px;
            font-weight: 700;
        }
        QLabel#emptyStateSubtitle {
            font-size: 16px;
            color: #c0c0c0;
            max-width: 600px;
        }
        QPushButton#bigAddButton {
            font-size: 16px;
            font-weight: 600;
            padding: 10px 20px;
        }
    )";

    app.setStyleSheet(style);

    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

    MainWindow w;
    w.resize(1200, 800);
    w.show();

    return app.exec();
}
