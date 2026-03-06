#pragma once

#include <QWidget>
#include <QTableView>
#include <QLineEdit>
#include <QComboBox>
#include <QSortFilterProxyModel>
#include "devicemodel.h"

class DevicesPage : public QWidget {
    Q_OBJECT
public:
    explicit DevicesPage(DeviceModel *model, QWidget *parent = nullptr);

private:
    void setupUI();
    void onDeviceDoubleClicked(const QModelIndex &index);

    DeviceModel *m_model;
    QTableView *m_tableView;
    QLineEdit *m_searchBox;
    QSortFilterProxyModel *m_proxyModel;
};
