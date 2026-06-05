#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QHeaderView>
#include <QTabWidget>
#include <QTextEdit>
#include <QProcess>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QCheckBox>
#include <QDialog>
#include <unistd.h>

class OpenRCManager : public QMainWindow {
    Q_OBJECT
public:
    OpenRCManager() {
        setWindowTitle("OpenRC Service Manager GUI (Qt6)");
        resize(850, 550);

        if (geteuid() != 0) {
            QMessageBox::critical(this, "Privilege Error", "This application must be run as ROOT.");
            exit(1);
        }

        tabs = new QTabWidget(this);
        setCentralWidget(tabs);

        initServicesTab();
        initLogsTab();

        logFilePath = "/var/log/rc.log";
        lastLogSize = 0;
        
        logTimer = new QTimer(this);
        connect(logTimer, &QTimer::timeout, this, &OpenRCManager::updateLogsRealtime);
        logTimer->start(1000);
    }

private slots:
    void loadServices() {
        table->setRowCount(0);
        QProcess process;
        process.start("rc-status", QStringList() << "--all");
        if (!process.waitForFinished()) return;

        QString output = process.readAllStandardOutput();
        QString currentRunlevel = "None";

        QStringList lines = output.split('\n');
        for (QString line : lines) {
            line = line.trimmed();
            if (line.isEmpty()) continue;

            if (line.contains("Runlevel:") || line.startsWith("*")) {
                currentRunlevel = line.replace("*", "").replace("Runlevel:", "").trimmed();
                continue;
            }

            if (line.contains("[") && line.contains("]")) {
                QStringList parts = line.split("[");
                QString serviceName = parts[0].trimmed();
                QString status = parts[1].replace("]", "").trimmed();

                int row = table->rowCount();
                table->insertRow(row);
                table->setItem(row, 0, new QTableWidgetItem(serviceName));
                table->setItem(row, 1, new QTableWidgetItem(currentRunlevel));
                table->setItem(row, 2, new QTableWidgetItem(status));
            }
        }
    }

    void runAction(QString action) {
        int row = table->currentRow();
        if (row < 0) {
            QMessageBox::warning(this, "Warning", "Please select a service in the table first.");
            return;
        }
        QString service = table->item(row, 0)->text();
        QProcess::execute("rc-service", QStringList() << service << action);
        loadServices();
    }

    void addService() {
        QString service = inputService->text().trimmed();
        if (service.isEmpty()) {
            QMessageBox::warning(this, "Warning", "Enter a valid script name from /etc/init.d/");
            return;
        }
        QProcess::execute("rc-update", QStringList() << "add" << service << "default");
        inputService->clear();
        loadServices();
    }

    void deleteService() {
        int row = table->currentRow();
        if (row < 0) return;
        QString service = table->item(row, 0)->text();
        QString runlevel = table->item(row, 1)->text();

        auto res = QMessageBox::question(this, "Confirmation", "Are you sure you want to delete the service '" + service + "'?", QMessageBox::Yes | QMessageBox::No);
        if (res == QMessageBox::Yes) {
            QProcess::execute("rc-update", QStringList() << "del" << service << runlevel);
            loadServices();
        }
    }

    void updateLogsRealtime() {
        QFileInfo checkFile(logFilePath);
        if (!checkFile.exists()) {
            logView->setText("File /var/log/rc.log is disabled or not found.");
            return;
        }

        qint64 currentSize = checkFile.size();
        if (currentSize != lastLogSize) {
            QFile file(logFilePath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                file.seek(lastLogSize);
                QTextStream in(&file);
                QString newLines = in.readAll();
                if (!newLines.isEmpty()) {
                    logView->append(newLines.trimmed());
                }
                lastLogSize = currentSize;
            }
        }
    }

private:
    QTabWidget *tabs;
    QTableWidget *table;
    QLineEdit *inputService;
    QTextEdit *logView;
    QTimer *logTimer;
    QString logFilePath;
    qint64 lastLogSize;

    void initServicesTab() {
        QWidget *tab = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(tab);

        QHBoxLayout *addLayout = new QHBoxLayout();
        inputService = new QLineEdit();
        inputService->setPlaceholderText("e.g. docker, nginx...");
        QPushButton *btnAdd = new QPushButton("➕ Add Service");
        connect(btnAdd, &QPushButton::clicked, this, &OpenRCManager::addService);
        
        addLayout->addWidget(new QLabel("Service Name:"));
        addLayout->addWidget(inputService);
        addLayout->addWidget(btnAdd);
        layout->addLayout(addLayout);

        table = new QTableWidget(0, 3);
        table->setHorizontalHeaderLabels(QStringList() << "Service" << "Runlevel" << "Status");
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        layout->addWidget(table);

        QHBoxLayout *ctrl = new QHBoxLayout();
        QPushButton *bStart = new QPushButton("▶ Start");
        QPushButton *bStop = new QPushButton("⏹ Stop");
        QPushButton *bDel = new QPushButton("❌ Delete");
        QPushButton *bRef = new QPushButton("🔄 Refresh");

        connect(bStart, &QPushButton::clicked, [this](){ runAction("start"); });
        connect(bStop, &QPushButton::clicked, [this](){ runAction("stop"); });
        connect(bDel, &QPushButton::clicked, this, &OpenRCManager::deleteService);
        connect(bRef, &QPushButton::clicked, this, &OpenRCManager::loadServices);

        ctrl->addWidget(bStart); ctrl->addWidget(bStop); ctrl->addWidget(bDel); ctrl->addWidget(bRef);
        layout->addLayout(ctrl);

        tabs->addTab(tab, "⚙ Services");
        loadServices();
    }

    void initLogsTab() {
        QWidget *tab = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(tab);
        logView = new QTextEdit();
        logView->setReadOnly(true);
        logView->setStyleSheet("background-color: #1a1a1a; color: #00ff00; font-family: monospace;");
        layout->addWidget(logView);
        tabs->addTab(tab, "📋 Boot Logs");
    }
};

// Função para mostrar a tela de Aviso de Responsabilidade antes de carregar o app
bool showDisclaimer() {
    QDialog dialog;
    dialog.setWindowTitle("Security Warning");
    dialog.resize(400, 200);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *text = new QLabel(
        "<b>WARNING:</b> Please use this software with extreme responsibility.<br><br>"
        "Modifying, stopping, or deleting critical system services can prevent your "
        "operating system from booting next time.<br><br>"
        "Are you sure you want to proceed?", &dialog);
    text->setWordWrap(true);
    layout->addWidget(text);

    QCheckBox *checkbox = new QCheckBox("Yes, I will be careful", &dialog);
    layout->addWidget(checkbox);

    QHBoxLayout *buttons = new QHBoxLayout();
    QPushButton *btnCancel = new QPushButton("Exit", &dialog);
    QPushButton *btnProceed = new QPushButton("Proceed", &dialog);
    btnProceed->setEnabled(false); // Fica desativado por padrão

    buttons->addWidget(btnCancel);
    buttons->addWidget(btnProceed);
    layout->addLayout(buttons);

    // O botão Proceed só liga quando a caixinha estiver marcada
    QObject::connect(checkbox, &QCheckBox::toggled, btnProceed, &QPushButton::setEnabled);
    
    QObject::connect(btnCancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(btnProceed, &QPushButton::clicked, &dialog, &QDialog::accept);

    return dialog.exec() == QDialog::Accepted;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Se o usuário fechar a janela ou desistir, o app não inicia
    if (!showDisclaimer()) {
        return 0;
    }

    OpenRCManager w;
    w.show();
    return app.exec();
}

#include "main.moc"
