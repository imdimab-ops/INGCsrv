#include "uiviewmapping.h"
#include "ui_uiviewmapping.h"

UiViewMapping::UiViewMapping(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::UiViewMapping)
{
    ui->setupUi(this);
    this->setWindowTitle("View mapping tags");
    setupUI();
    setupConnections();
    loadMapping();
}

UiViewMapping::~UiViewMapping()
{
    delete ui;
}

void UiViewMapping::setupUI()
{
    ui->pb_openDir->setIcon(QIcon(":img/icon/mainmenu/openfolder.svg"));
    ui->pb_loadMap->setIcon(QIcon(":img/icon/load.svg"));
    ui->pb_clearMap->setIcon(QIcon(":img/icon/clean.svg"));
    ui->pb_clearEvent->setIcon(QIcon(":img/icon/clean.svg"));
    ui->pb_close->setIcon(QIcon(":img/icon/mainmenu/close.svg"));
    ui->pb_expand->setIcon(QIcon(":img/icon/expandAll.svg"));
    ui->pb_collapse->setIcon(QIcon(":img/icon/collapseAll.svg"));

    // ui->treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents); //данное свойсвто приводит к тормозам Ui в момент обновления при перемещении виджета!!!
    ui->treeView->header()->setMinimumSectionSize(80);
    ui->treeView->header()->setDefaultAlignment(Qt::AlignCenter);
    ui->treeView->setModel(modelMapping);
    ui->treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->treeView->setColumnWidth(ui->treeView->model()->columnCount() - 1, 0); //размер последней колонки
    ui->treeView->setAlternatingRowColors(true);
    ui->treeView->setStyleSheet(appLib->qss.treeView_height25); //ширина строк
    ui->treeView->setUniformRowHeights(true); //единая высота для всех строк

    //Drag&Drop (запрещаем перетаскивание внутри модели
    ui->treeView->setDragEnabled(false);
    ui->treeView->setAcceptDrops(false);
    ui->treeView->setDropIndicatorShown(false);
    ui->treeView->setDragDropMode(QAbstractItemView::NoDragDrop);
    ui->treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    //splitter
    ui->splitter->setStretchFactor(0, 6);   //соотношение 6:4
    ui->splitter->setStretchFactor(1, 4);

    //tableView
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->setModel(modelEvent);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->horizontalHeader()->setStretchLastSection(true); //растянуть последнюю колонку
    ui->tableView->verticalHeader()->setDefaultSectionSize(20); //20px
}

void UiViewMapping::setupConnections()
{
    connect(ui->pb_openDir, &QPushButton::clicked, this, [this]() {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(FileIO::getConfigDir()))) {
            modelSysLog->append(Q_FUNC_INFO, QString("[Error] filed open dir:%1").arg(FileIO::getConfigDir()));
        };
    });
    connect(ui->pb_loadMap, &QPushButton::clicked, this, [this]() {
        QString fileName = QFileDialog::getOpenFileName(
            this,
            tr("Выберите файл для загрузки"),
            FileIO::getConfigDir(),  //начальная директория
            tr("JSON файлы (*.json);;Все файлы (*.*)")  //фильтры
            );
        if (fileName.isEmpty()) return;
        modelMapping->clear();
        ModelMapping *newMapping = JsonModelIO::loadModelFromJson(fileName); //экземпляр не создали т.к. метод saveModelToJson статический
        if (newMapping != nullptr) {
            modelMapping = newMapping;
            modelMapping->saveMapping(); //при вызове метода saveMapping(), из модели формируется сигнал emit nodeIdsCompleted(nodeIds);
        }
    });
    connect(ui->pb_clearMap, &QPushButton::clicked, this, [this]() {
        dialog->setCaller(this);
        dialog->setWindowTitle("Clear");
        dialog->setText("Очистить карту привязки тегов?");
        if(dialog->exec() == QDialog::Rejected) {   //вызываем диалог для подтверждения
            return;
        };
        modelMapping->clear();
    });
    connect(ui->pb_clearEvent, &QPushButton::clicked, this, [this]() { modelEvent->clear(); });
    connect(ui->pb_close, &QPushButton::clicked, this, [this]() { this->close(); });
    connect(ui->pb_expand, &QPushButton::clicked, this, [this]() { ui->treeView->expandAll(); });
    connect(ui->pb_collapse, &QPushButton::clicked, this, [this]() { ui->treeView->collapseAll(); });

    //отслеживаем изменение модели
    connect(modelMapping, &ModelMapping::modelCompleted, this, [this]() {});
}

void UiViewMapping::loadMapping()
{
    //загружаем карту привязки тэгов и сохраняем
    ModelMapping *newMapping = JsonModelIO::loadModelFromJson(); //экземпляр не создали т.к. метод saveModelToJson статический
    if (newMapping != nullptr) {
        modelMapping = newMapping;
        modelMapping->saveMapping(); //при вызове метода saveMapping(), из модели формируется сигнал emit nodeIdsCompleted(nodeIds);
    }
}

void UiViewMapping::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
}

void UiViewMapping::moveEvent(QMoveEvent *event)
{

}
