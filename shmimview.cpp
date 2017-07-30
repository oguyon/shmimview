#include "shmimview.h"
#include "ui_shmimview.h"

#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/mman.h> // mmap, PROT_READ, PROT_WRITE ...


#include <semaphore.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close

#include <ImageStruct.h>


#include <QtGlobal>
#include <QImage>
#include <QTimer>



IMAGE ImageData;
IMAGE ImageDataDark;

float *imdataf = NULL;
unsigned char *imdata = NULL;
uint8_t imtype;

// data values
float dataMaxVal;
float dataMinVal;

int autominmax = 0;


QImage qimage(100, 100,  QImage::Format_Grayscale8);




long read_sharedmem_image_toIMAGE(const char *name, IMAGE *image);



static QGraphicsScene * scene;
static int initscene = 0;
static long long refreshcnt = 0;
static int imxsize_view;
static int imysize_view;
static float freq;
static float minVal = 0.0;
static float maxVal = 1.0;
static long refreshintervalms = 1000;
static int brightness_scale_mode = 0;
static int OKdark = 0;
static int rmDarkMode = 0;

static long long imcnt0, imcnt1;
long long imcnt00 = 0;
static long imxsize, imysize, imzsize;

QTimer *timer;
char tmpstring[200];
char input_streamname[200];
float input_framerate;

float input_minval;
float input_maxval;
int imxsize_view_min = 200;
int imysize_view_min = 200;
int imxsize_view_max = 1000;
int imysize_view_max = 1000;


int fileconfinit = 0; // toggles to 1 if parameter file present



// Reads from disk input parameters
// Default configuration file : .shmimview.conf
// File format :
// Line 1: stream name
// Line 2: update frequency

int ReadFileParameters()
{
    FILE *fp;
    char keyword[200];
    char fieldval[200];

    fp = fopen(".shmimview.conf", "r");
    if(fp==NULL)
    {
        printf("Warning : no input parameter conf file\n");
        fflush(stdout);
    }
    else
    {
        printf("Found parameter conf file\n");
        fflush(stdout);

        fileconfinit = 1;

        while(fscanf(fp, "%s %s", keyword, fieldval)==2)
        {
            printf("keyword = %20s   value = %20s\n", keyword, fieldval);

            if(!strcmp(keyword, "STREAM_NAME"))
                strcpy(input_streamname, fieldval);
            if(!strcmp(keyword, "FRAME_RATE"))
                input_framerate = atof(fieldval);
            if(!strcmp(keyword, "VAL_MIN"))
                input_minval = atof(fieldval);
            if(!strcmp(keyword, "VAL_MAX"))
                input_maxval = atof(fieldval);
        }
        fclose(fp);
    }

    return 0;
}







// This function is executed upon startup
shmimview::shmimview(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::shmimview)
{
    ui->setupUi(this);

    this->setStyleSheet(
                    "QToolButton#streamON::checked {"
                    "background-color: red; }"

                    "QToolButton#rmdarkON::checked {"
                    "background-color: red; }"
               );



    ui->brightnessscale->addItem("Linear");
    ui->brightnessscale->addItem("Log");
    ui->brightnessscale->addItem("Sqrt");
    ui->brightnessscale->addItem("Sqr");
    ui->brightnessscale->addItem("Pow01");
    ui->brightnessscale->addItem("Pow02");
    ui->brightnessscale->addItem("Pow04");
    ui->brightnessscale->addItem("Pow08");
    ui->brightnessscale->addItem("Pow16");
    ui->brightnessscale->addItem("Pow32");



    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(on_display_clicked()));



    // STARTUP


    // Default values
    strcpy(input_streamname, "nullim");

    ReadFileParameters();

    // UPDATE FREQUENCY
    sprintf(tmpstring, "%.2f", input_framerate);
    ui->updatefreq->setText(tmpstring);
    freq = atof(qPrintable(ui->updatefreq->text()));
    refreshintervalms = (long) (1000.0/freq);

    // STREAM NAME
    ui->streamname->setText(input_streamname);


    sprintf(tmpstring, "%.3f", input_minval);
    ui->minval->setText(tmpstring);
    minVal = input_minval;

    sprintf(tmpstring, "%.3f", input_maxval);
    ui->maxval->setText(tmpstring);
    maxVal = input_maxval;



 //   timer->start(refreshintervalms);
}



shmimview::~shmimview()
{
    delete ui;
}


void shmimview::on_brightnessscale_currentIndexChanged()
{
    brightness_scale_mode = ui->brightnessscale->currentIndex();

    printf("CURRENT SELECTION CHANGED to : %d\n", brightness_scale_mode);
    fflush(stdout);
}


void shmimview::on_setfreq_clicked()
{
    char messagetext[200];

    freq = atof(qPrintable(ui->updatefreq->text()));
    sprintf(messagetext, "set update freq to %.2f Hz", freq);
    refreshintervalms = (long) (1000.0/freq);

    timer->stop();

    timer->start(refreshintervalms);

    ui->msgbox->append(messagetext);
}



void shmimview::on_setminval_clicked()
{
    char messagetext[200];

    minVal = atof(qPrintable(ui->minval->text()));
    sprintf(messagetext, "set min val to  %.2f", minVal);

    ui->msgbox->append(messagetext);
}


void shmimview::on_setmaxval_clicked()
{
    char messagetext[200];

    maxVal = atof(qPrintable(ui->maxval->text()));
    sprintf(messagetext, "set max val to  %.2f", maxVal);

    ui->msgbox->append(messagetext);
}



void shmimview::on_streamON_clicked()
{
    if(imdata==NULL)
    {
       ui->streamON->setChecked(0);
       timer->stop();
    }
    else
    {
    printf("button streamON clicked\n");
    if(ui->streamON->isChecked()==0)
        timer->stop();
    else
        timer->start(refreshintervalms);
    }
}


void shmimview::on_rmdarkON_clicked()
{
    if(OKdark==0)
    {
       ui->rmdarkON->setChecked(0);
    }
    else
    {
    if(ui->rmdarkON->isChecked()==0)
        rmDarkMode = 0;
    else
        rmDarkMode = 1;
    }
}


void shmimview::on_autominmax_clicked()
{
    if(imdata==NULL)
    {
       ui->autominmax->setChecked(0);
       autominmax = 0;
    }
    else
    {
    if(ui->autominmax->isChecked()==0)
        autominmax = 0;
    else
        autominmax = 1;
    }
}




void shmimview::data2bitmap()
{
    long ii, jj;
    long iii;

    float dataMaxVal = 1.0;
    float dataMinVal = 0.0;

    // convert to float
    switch (imtype) {

    case (_DATATYPE_UINT8):
    for(ii=0;ii<imxsize;ii++)
       for(jj=0;jj<imysize;jj++)
          imdataf[jj*imxsize+ii] = 1.0*ImageData.array.UI8[jj*imxsize+ii];
    break;

    case (_DATATYPE_INT8):
    for(ii=0;ii<imxsize;ii++)
       for(jj=0;jj<imysize;jj++)
          imdataf[jj*imxsize+ii] = 1.0*ImageData.array.SI8[jj*imxsize+ii];
    break;

    case (_DATATYPE_UINT16):
    for(ii=0;ii<imxsize;ii++)
       for(jj=0;jj<imysize;jj++)
          imdataf[jj*imxsize+ii] = 1.0*ImageData.array.UI16[jj*imxsize+ii];
    break;

    case (_DATATYPE_INT16):
    for(ii=0;ii<imxsize;ii++)
       for(jj=0;jj<imysize;jj++)
          imdataf[jj*imxsize+ii] = 1.0*ImageData.array.SI16[jj*imxsize+ii];
    break;

    case (_DATATYPE_UINT32):
    for(ii=0;ii<imxsize;ii++)
       for(jj=0;jj<imysize;jj++)
          imdataf[jj*imxsize+ii] = 1.0*ImageData.array.UI32[jj*imxsize+ii];
    break;

    case (_DATATYPE_INT32):
    for(ii=0;ii<imxsize;ii++)
       for(jj=0;jj<imysize;jj++)
          imdataf[jj*imxsize+ii] = 1.0*ImageData.array.SI32[jj*imxsize+ii];
    break;

    case (_DATATYPE_UINT64):
    for(ii=0;ii<imxsize;ii++)
       for(jj=0;jj<imysize;jj++)
          imdataf[jj*imxsize+ii] = 1.0*ImageData.array.UI64[jj*imxsize+ii];
    break;

    case (_DATATYPE_INT64):
    for(ii=0;ii<imxsize;ii++)
       for(jj=0;jj<imysize;jj++)
          imdataf[jj*imxsize+ii] = 1.0*ImageData.array.SI64[jj*imxsize+ii];
    break;



    case (_DATATYPE_FLOAT):
    for(ii=0;ii<imxsize;ii++)
        for(jj=0;jj<imysize;jj++)
           imdataf[jj*imxsize+ii] = 1.0*ImageData.array.F[jj*imxsize+ii];
        break;

    case (_DATATYPE_DOUBLE):
    for(ii=0;ii<imxsize;ii++)
        for(jj=0;jj<imysize;jj++)
           imdataf[jj*imxsize+ii] = 1.0*ImageData.array.D[jj*imxsize+ii];
        break;


    }

    if(rmDarkMode==1)
        for(iii=0;iii<imxsize*imysize;iii++)
            imdataf[iii] -= ImageDataDark.array.F[iii];


    dataMinVal = imdataf[0];
    dataMaxVal = imdataf[0];
    for(iii=0;iii<imxsize*imysize;iii++)
    {
        if(imdataf[iii]>dataMaxVal)
            dataMaxVal = imdataf[iii];
        if(imdataf[iii]<dataMinVal)
            dataMinVal = imdataf[iii];
    }

    if(autominmax==1)
    {
        minVal = dataMinVal;
        maxVal = dataMaxVal;
        sprintf(tmpstring, "%8g", minVal);
        ui->minval->setText(tmpstring);
        sprintf(tmpstring, "%8g", maxVal);
        ui->maxval->setText(tmpstring);
    }




    // clip 0.001 -> 1.0
    for(iii=0;iii<imxsize*imysize;iii++)
    {
        imdataf[iii] = (1.0*imdataf[iii]-minVal) / (maxVal-minVal);
        if(imdataf[iii]<0.000001)
            imdataf[iii] = 0.000001;
        if(imdataf[iii]>1.0)
            imdataf[iii] = 1.0;
    }


    switch ( brightness_scale_mode ) {

    case ( 1 ) : // log scale
        for(iii=0 ; iii<imxsize*imysize ; iii++)
            imdataf[iii] = (log10(imdataf[iii]+0.00001)+5.0)/5.0;
        break;

    case ( 2 ) : // sqrt
        for(iii=0 ; iii<imxsize*imysize ; iii++)
            imdataf[iii] = sqrt(imdataf[iii]);
        break;

    case ( 3 ) : // sqr
        for(iii=0 ; iii<imxsize*imysize ; iii++)
            imdataf[iii] = imdataf[iii]*imdataf[iii];
        break;

    case ( 4 ) : // pow 01
        for(iii=0 ; iii<imxsize*imysize ; iii++)
            imdataf[iii] = pow(imdataf[iii], 0.1);
        break;

    case ( 5 ) : // pow 02
        for(iii=0 ; iii<imxsize*imysize ; iii++)
            imdataf[iii] = pow(imdataf[iii], 0.2);
        break;

    case ( 6 ) : // pow 04
        for(iii=0 ; iii<imxsize*imysize ; iii++)
            imdataf[iii] = pow(imdataf[iii], 0.4);
        break;

    case ( 7 ) : // pow 08
        for(iii=0 ; iii<imxsize*imysize ; iii++)
            imdataf[iii] = pow(imdataf[iii], 0.8);
        break;

    case ( 8 ) : // pow 16
        for(iii=0 ; iii<imxsize*imysize ; iii++)
            imdataf[iii] = pow(imdataf[iii], 1.6);
        break;

    case ( 9 ) : // pow 32
        for(iii=0 ; iii<imxsize*imysize ; iii++)
            imdataf[iii] = pow(imdataf[iii], 3.2);
        break;


    }


    for(ii=0;ii<imxsize;ii++)
        for(jj=0;jj<imysize;jj++)
           imdata[jj*imxsize+ii] = (unsigned char) (imdataf[jj*imxsize+ii] * 255.0 );

}



void shmimview::on_display_clicked()
{
    QPixmap qpixmap;
    char messagetext[200];

    imcnt0 = ImageData.md[0].cnt0;

    if(imcnt0!=imcnt00)
    {
    ui->cnt0->setText(QString::number(imcnt0));
    imcnt1 = ImageData.md[0].cnt1;
    ui->cnt1->setText(QString::number(imcnt1));

    data2bitmap();

    QImage qimage(imdata, imxsize, imysize, QImage::Format_Grayscale8);

    qpixmap = QPixmap::fromImage(qimage);



    scene->clear();
    scene->addPixmap(qpixmap);

    refreshcnt++;
    imcnt00 = imcnt0;

    sprintf(messagetext, "[%10lld] display updated %d %d", refreshcnt, OKdark, rmDarkMode);
    ui->msgbox->append(messagetext);
    }
}





void shmimview::on_connect_clicked()
{
    char sname[200];
    char snamedark[200];
    char messagetext[200];
    int imnaxis;


    sprintf(sname, "%s", qPrintable(ui->streamname->text()));



    if( read_sharedmem_image_toIMAGE(sname, &ImageData) == -1)
    {
        sprintf(messagetext, "ERROR: cannot load stream %s", sname);
        ui->msgbox->append(messagetext);
    }
    else
    {
        ui->stream->setText(sname);

        sprintf(messagetext, "[%10lld] Stream %s loaded (%.2f Hz)", refreshcnt, sname, freq);
        ui->msgbox->append(messagetext);


        imtype = ImageData.md[0].atype;
        switch (imtype) {

        case (_DATATYPE_UINT8):
        ui->type->setText("UINT8");
        break;

        case (_DATATYPE_INT8):
        ui->type->setText("INT8");
        break;

        case (_DATATYPE_UINT16):
        ui->type->setText("UINT16");
        break;

        case (_DATATYPE_INT16):
        ui->type->setText("INT16");
        break;

        case (_DATATYPE_UINT32):
        ui->type->setText("UINT32");
        break;

        case (_DATATYPE_INT32):
        ui->type->setText("INT32");
        break;

        case (_DATATYPE_UINT64):
        ui->type->setText("UINT64");
        break;

        case (_DATATYPE_INT64):
        ui->type->setText("INT64");
        break;

        case (_DATATYPE_FLOAT):
        ui->type->setText("FLOAT");
        break;

        case (_DATATYPE_DOUBLE):
        ui->type->setText("DOUBLE");
        break;

        default:
        ui->type->setText("ERR");
        break;
        }


        imnaxis = ImageData.md[0].naxis;
        ui->naxis->setText(QString::number(imnaxis));

        imxsize = ImageData.md[0].size[0];
        ui->xsize->setText(QString::number(imxsize));

        if(imnaxis>1)
        {
        imysize = ImageData.md[0].size[1];
        ui->ysize->setText(QString::number(imysize));
        }
        else
         ui->ysize->setText("-");

        if(imnaxis>2)
        {
        imzsize = ImageData.md[0].size[2];
        ui->zsize->setText(QString::number(imzsize));
        }
        else
         ui->zsize->setText("-");


        imcnt0 = ImageData.md[0].cnt0;
        ui->cnt0->setText(QString::number(imcnt0));
        imcnt1 = ImageData.md[0].cnt1;
        ui->cnt1->setText(QString::number(imcnt1));


        // SET VIEWBOX SIZE
        imxsize_view = imxsize;
        if(imxsize_view < imxsize_view_min)
            imxsize_view = imxsize_view_min;
        if(imxsize_view > imxsize_view_max)
            imxsize_view = imxsize_view_max;

        imysize_view = imysize;
        if(imysize_view < imysize_view_min)
            imysize_view = imysize_view_min;
        if(imysize_view > imysize_view_max)
            imysize_view = imysize_view_max;


        ui->viewbox->setFixedHeight(imxsize_view);
        ui->viewbox->setFixedWidth(imysize_view);



        imdataf = (float*) malloc(sizeof(float)*imxsize*imysize);
        imdata = (unsigned char*) malloc(sizeof(char)*imxsize*imysize);
        data2bitmap();

        QImage qimage(imdata, imxsize, imysize, QImage::Format_Grayscale8);

        //QGraphicsScene scene;
        if(initscene==0){
            scene = new QGraphicsScene();
            initscene = 1;
        }
        scene->addPixmap(QPixmap::fromImage(qimage));
        ui->viewbox->setScene(scene);
        refreshcnt++;

        ui->streamON->setChecked(0);
        timer->stop();

      //  timer->start(refreshintervalms);
    }

    // load dark if exists
    OKdark = 0;
    sprintf(snamedark, "%s_dark", qPrintable(ui->streamname->text()));

    if( read_sharedmem_image_toIMAGE(snamedark, &ImageDataDark) == -1)
    {
        sprintf(messagetext, "WARNING: cannot load dark %s", snamedark);
        ui->msgbox->append(messagetext);
    }
    else
    {
        sprintf(messagetext, "Loaded dark %s", snamedark);
        ui->msgbox->append(messagetext);
        OKdark = 1;
    }

    if(OKdark == 1)
    if(ImageDataDark.md[0].atype != _DATATYPE_FLOAT )
    {
        sprintf(messagetext, "ERROR: dark is not float");
        ui->msgbox->append(messagetext);
        OKdark = 0;
    }

}



void shmimview::on_VBbigger_clicked()
{
    imxsize_view = (int) (1.2*imxsize_view);
    if(imxsize_view > imxsize_view_max)
       imxsize_view = imxsize_view_max;

    imysize_view = (int) (1.2*imysize_view);
    if(imysize_view > imysize_view_max)
        imysize_view = imysize_view_max;
    ui->viewbox->setFixedHeight(imxsize_view);
    ui->viewbox->setFixedWidth(imysize_view);
}


void shmimview::on_VBsmaller_clicked()
{
    imxsize_view = (int) (1.0/1.2*imxsize_view);
    if(imxsize_view > imxsize_view_max)
       imxsize_view = imxsize_view_max;
    if(imxsize_view < imxsize_view_min)
       imxsize_view = imxsize_view_min;

    imysize_view = (int) (1.0/1.2*imysize_view);
    if(imysize_view < imysize_view_min)
        imysize_view = imysize_view_min;
    if(imysize_view > imysize_view_max)
        imysize_view = imysize_view_max;



    ui->viewbox->setFixedHeight(imxsize_view);
    ui->viewbox->setFixedWidth(imysize_view);

    resize(minimumSize());
}





void shmimview::wheelEvent(QWheelEvent *event){

        ui->viewbox->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

        // Scale the view / do the zoom
        double scaleFactor = 1.15;
        if(event->delta() > 0) {
            // Zoom in
            ui->viewbox->scale(scaleFactor, scaleFactor);

        } else {
            // Zooming out
             ui->viewbox->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
        }
}





long read_sharedmem_image_toIMAGE(const char *name, IMAGE *image)
{
    int SM_fd;
    struct stat file_stat;
    char SM_fname[200];
    IMAGE_METADATA *map;
    char *mapv;
    uint8_t atype;
    int kw;
    char sname[200];
    sem_t *stest;
    int sOK;
    long snb;
    long s;

   int rval = -1;



    sprintf(SM_fname, "%s/%s.im.shm", SHAREDMEMDIR, name);

    SM_fd = open(SM_fname, O_RDWR);
    if(SM_fd==-1)
    {
        image->used = 0;
        return(-1);
        printf("Cannot import shared memory file %s \n", name);
        rval = -1;
    }
    else
    {
        rval = 0; // we assume by default success

        fstat(SM_fd, &file_stat);
        printf("File %s size: %zd\n", SM_fname, file_stat.st_size);




        map = (IMAGE_METADATA*) mmap(0, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, SM_fd, 0);
        if (map == MAP_FAILED) {
            close(SM_fd);
            perror("Error mmapping the file");
            rval = -1;
            exit(0);
        }



        image->memsize = file_stat.st_size;

        image->shmfd = SM_fd;



        image->md = map;
       // image->md[0].sem = 0;
        atype = image->md[0].atype;
        image->md[0].shared = 1;


        printf("image size = %ld %ld\n", (long) image->md[0].size[0], (long) image->md[0].size[1]);
        fflush(stdout);
        // some verification
        if(image->md[0].size[0]*image->md[0].size[1]>10000000000)
        {
            printf("IMAGE \"%s\" SEEMS BIG... ABORTING\n", name);
            rval = -1;
            exit(0);
        }
        if(image->md[0].size[0]<1)
        {
            printf("IMAGE \"%s\" AXIS SIZE < 1... ABORTING\n", name);
            rval = -1;
            exit(0);
        }
        if(image->md[0].size[1]<1)
        {
            printf("IMAGE \"%s\" AXIS SIZE < 1... ABORTING\n", name);
            rval = -1;
            exit(0);
        }



        mapv = (char*) map;
        mapv += sizeof(IMAGE_METADATA);



        printf("atype = %d\n", (int) atype);
        fflush(stdout);

        if(atype == _DATATYPE_UINT8)
        {
            printf("atype = UINT8\n");
            image->array.UI8 = (uint8_t*) mapv;
            mapv += SIZEOF_DATATYPE_UINT8 * image->md[0].nelement;
        }

        if(atype == _DATATYPE_INT8)
        {
            printf("atype = INT8\n");
            image->array.SI8 = (int8_t*) mapv;
            mapv += SIZEOF_DATATYPE_INT8 * image->md[0].nelement;
        }

        if(atype == _DATATYPE_UINT16)
        {
            printf("atype = UINT16\n");
            image->array.UI16 = (uint16_t*) mapv;
            mapv += SIZEOF_DATATYPE_UINT16 * image->md[0].nelement;
        }

        if(atype == _DATATYPE_INT16)
        {
            printf("atype = INT16\n");
            image->array.SI16 = (int16_t*) mapv;
            mapv += SIZEOF_DATATYPE_INT16 * image->md[0].nelement;
        }

        if(atype == _DATATYPE_UINT32)
        {
            printf("atype = UINT32\n");
            image->array.UI32 = (uint32_t*) mapv;
            mapv += SIZEOF_DATATYPE_UINT32 * image->md[0].nelement;
        }

        if(atype == _DATATYPE_INT32)
        {
            printf("atype = INT32\n");
            image->array.SI32 = (int32_t*) mapv;
            mapv += SIZEOF_DATATYPE_INT32 * image->md[0].nelement;
        }

        if(atype == _DATATYPE_UINT64)
        {
            printf("atype = UINT64\n");
            image->array.UI64 = (uint64_t*) mapv;
            mapv += SIZEOF_DATATYPE_UINT64 * image->md[0].nelement;
        }

        if(atype == _DATATYPE_INT64)
        {
            printf("atype = INT64\n");
            image->array.SI64 = (int64_t*) mapv;
            mapv += SIZEOF_DATATYPE_INT64 * image->md[0].nelement;
        }

        if(atype == _DATATYPE_FLOAT)
        {
            printf("atype = FLOAT\n");
            image->array.F = (float*) mapv;
            mapv += SIZEOF_DATATYPE_FLOAT * image->md[0].nelement;
        }

        if(atype == _DATATYPE_DOUBLE)
        {
            printf("atype = DOUBLE\n");
            image->array.D = (double*) mapv;
            mapv += SIZEOF_DATATYPE_COMPLEX_DOUBLE * image->md[0].nelement;
        }

        if(atype == _DATATYPE_COMPLEX_FLOAT)
        {
            printf("atype = COMPLEX_FLOAT\n");
            image->array.CF = (complex_float*) mapv;
            mapv += SIZEOF_DATATYPE_COMPLEX_FLOAT * image->md[0].nelement;
        }

        if(atype == _DATATYPE_COMPLEX_DOUBLE)
        {
            printf("atype = COMPLEX_DOUBLE\n");
            image->array.CD = (complex_double*) mapv;
            mapv += SIZEOF_DATATYPE_COMPLEX_DOUBLE * image->md[0].nelement;
        }



        printf("%ld keywords\n", (long) image->md[0].NBkw);
        fflush(stdout);

        image->kw = (IMAGE_KEYWORD*) (mapv);

        for(kw=0; kw<image->md[0].NBkw; kw++)
        {
            if(image->kw[kw].type == 'L')
                printf("%d  %s %ld %s\n", kw, image->kw[kw].name, image->kw[kw].value.numl, image->kw[kw].comment);
            if(image->kw[kw].type == 'D')
                printf("%d  %s %lf %s\n", kw, image->kw[kw].name, image->kw[kw].value.numf, image->kw[kw].comment);
            if(image->kw[kw].type == 'S')
                printf("%d  %s %s %s\n", kw, image->kw[kw].name, image->kw[kw].value.valstr, image->kw[kw].comment);
        }


        mapv += sizeof(IMAGE_KEYWORD)*image->md[0].NBkw;

        strcpy(image->name, name);




        // looking for semaphores
        snb = 0;
        sOK = 1;
        while(sOK==1)
        {
            sprintf(sname, "%s_sem%02ld", image->md[0].name, snb);
            if((stest = sem_open(sname, 0, 0644, 0))== SEM_FAILED)
                sOK = 0;
            else
                {
                    sem_close(stest);
                    snb++;
                }
        }
        printf("%ld semaphores detected  %ld\n", snb, (long) image->md[0].sem);

        //image->md[0].sem = snb;

        image->semptr = (sem_t**) malloc(sizeof(sem_t*) * image->md[0].sem);
        for(s=0; s<snb; s++)
        {
            sprintf(sname, "%s_sem%02ld", image->md[0].name, s);
            if ((image->semptr[s] = sem_open(sname, 0, 0644, 0))== SEM_FAILED) {
                printf("ERROR: could not open semaphore %s\n", sname);
            }
        }

        sprintf(sname, "%s_semlog", image->md[0].name);
        if ((image->semlog = sem_open(sname, 0, 0644, 0))== SEM_FAILED) {
                printf("ERROR: could not open semaphore %s\n", sname);
            }

    }

    return(rval);
}




