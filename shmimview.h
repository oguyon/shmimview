#ifndef SHMIMVIEW_H
#define SHMIMVIEW_H

#include <QMainWindow>
#include <QGraphicsView>
#include <QWheelEvent>
#include <QKeyEvent>
#include <ImageStruct.h>


namespace Ui {
class shmimview;
}


class shmimview : public QMainWindow
{
    Q_OBJECT

protected:
    void wheelEvent(QWheelEvent*) override;

public:
    explicit shmimview(QWidget *parent = 0);
    ~shmimview();
    void data2bitmap();

private slots:
    void on_connect_clicked();
    void on_setfreq_clicked();
    void on_setminval_clicked();
    void on_setmaxval_clicked();
    void on_display_clicked();
    void on_streamON_clicked();
    void on_rmdarkON_clicked();
    void on_autominmax_clicked();
    void on_brightnessscale_currentIndexChanged();
    void on_VBbigger_clicked();
    void on_VBsmaller_clicked();

private:
    Ui::shmimview *ui;
};

#endif // SHMIMVIEW_H
