#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_QtLabViewTest.h"

class QtLabViewTest : public QMainWindow
{
    Q_OBJECT

public:
    QtLabViewTest(QWidget *parent = Q_NULLPTR);

private:
    Ui::QtLabViewTestClass ui;
};
