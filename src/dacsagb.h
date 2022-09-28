#pragma once
#include <cstdio>
#include <string>
#include <iostream>
//#include <filesystem>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cmath>
#include <climits>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <cassert>
#include <fstream>
#include <dirent.h>
using namespace std;

//namespace fs = std::filesystem;
#define VALORE_NUMERICO_NULLO  (double)9999999999.0000000
/* funzioni di utilit� generale*/
double dasag(double ang); //da sessaggesimali a centesimali
double dagas(double ang); //da  centesimali a sessaggesimali
double dasar(double ang); //da sessaggesimali a radianti
double dagar(double ang); //da centesimali a radianti
double daras(double ang); //da radianti a sessagesimali
double darag(double ang); //da radianti a centesimali
double dasdas(double ang); //da sessadecimali a sessaggesimali
double dasasd(double ang); //da sessadecimali a sessaggesimali
void angdir(double xs, double ys, double xt, double yt, double& dz, double& teta, double& cd, double& sd);
double adjang400(double ang);

/*parametri datum geodetico*/
#define awgs84 6378137.0e0
#define bwgs84  6356752.31414e0
#define e2wgs84 (awgs84* awgs84 -bwgs84 * bwgs84) / (awgs84 * awgs84)
#define ahi  6378388.0e0
#define bhi  6356911.946
#define e2hi (ahi* ahi - bhi * bhi) / ahi / ahi

/*specifica del formato degli angoli*/
#define i_u_s  0x00000000 //ingresso in sessaggesimali
#define i_u_r  0x00000001 //ingresso in radianti
#define i_u_c  0x00000002 //ingresso in centesimali
#define i_u_d  0x00000003 //ingresso in decimali

/* classe generale per i calcoli geodetici*/
class Geodesia 
{

public:
  double ang_conv(double ang, int un);
  double fis(double s, double alfa, int un = i_u_s);  //trasporto latitudine
  double las(double s, double alfa, int un = i_u_s);  //trasporto longitudine
  void fi_la(double s, double alfa, double& la, double& lo, int un = i_u_s);//s positivo o negativo alfa positivo
  void fi_la_step(double s, double alfa, double& la, double& lo, int un = i_u_s);//s positivo o negativo alfa positivo

  void s_alfa(double fi0, double la0, double fi1, double la1, double& s, double& az1, double& az2, int un = i_u_r);//s positivo o negativo alfa positivo
  void DaCSaGB(double fi_orig, double la_orig, double n, double e,
    double& arco_o, double& az1_bessel, double& az2_bessel,
    double& arco_e, double& az1_hayford, double& az2_hayford,
    double& lat_bessel, double& lon_bessel,
    double& lat_hayford, double& lon_hayford, int un = i_u_s
  );
  double quota;
  /*  latitudine sessagesimale. default    = 41.552551e0                 */
  /*  longitudine default                  = 12.270840                   */
  double ecc;     /*  eccentr. ellissoide default :Internazionale = 672267e-8              */
  double semiasse;/*  semias. maggiore (m)default:  = 6378388.e0                         */
  void SetAsse(double x = ahi) { semiasse = x; };  /* setta semiasse                        */
  void SetEcc(double x = e2hi) { ecc = x; };  /* setta eccentricita'                   */
  void SetEllWgs84() { SetAsse(awgs84); SetEcc(e2wgs84); }
  void convFiLa_da_wgs84_a_h(double f, double l, double q = 0);
  void convFiLa_da_h_a_wgs84(double f, double l, double q = 0);
  void calcoladaXYZ(double xx, double yy, double zz);
  Geodesia(); //inizializza tutti i valori sul default
  void calcola(int un = i_u_s, double fii = 41.552551, double lamd = 12.270840, double q = 0.0e0); //equivale a setta fi,_lambda
  double solvefi(double x, double y, double z, double f0);
  double xx(); //coordinate cartesiane del punto sull'ellisoide
  double yy();
  double zz();
  double xxq(); //coordinate cartesiane del punto in quota
  double yyq();
  double zzq();
  double fi; //sempre in radianti
  double _lambda; //sempre in radianti
  double W;
  double RG;
  double GN;
  double ro;
  double r;
  double x, y, z; //cordinate geocentriche correnti
               //calcolate esplicitamente chiamando xx,yy,zz 
  double xq, yq, zq; //cordinate geocentriche correnti
                //calcolate esplicitamente chiamando xq,yq,zq 


  double NordFE, EstFE;   //coordinate Gauss-Boaga su ellissoide Hayford fuso est
  double NordFO, EstFO;   //coordinate Gauss-Boaga su ellissoide Hayford fuso oves
  double NordUTM, EstUTM;   //coordinate UTM ETRF2000 
  double crid;           //coefficiente di contrazione 
  double lfi(); //menbro integrale ellittico
  void NEgb(double f, double l, bool MM = true); //Nord Est Gauss-Boaga  l relativa a MomteMario se MM=true
  void NEutm(double f, double l, int & fuso); //Nord Est UTM  l relativa a Greenwitch
  void FiLagb(double N, double E, double& fi, double& la); //calcola fi e lamda hayford da nORD ED eST gb
  void DaCSaWgs84(double n, double e, double q, double* xw, double* yw, double* zw);                                       //                      l relativa a Greenwich se MM=FALSE     
  void DaGBaWgs84(double n, double e, double q, double* xw, double* yw, double* zw);                                       //                      l relativa a Greenwich se MM=FALSE     
  double xcentrohay, ycentrohay, zcentrohay; //cordinate del centro di hayford in wgs84
  double sec(double x);
  double cosec(double x);
  double lat_geoc();
  double lat_ridotta(double lat);
  //virtual ~Geodesia();

};








#define dapianeapiane 0x00000001
#define dapianeageogr 0x00000002
#define dageograpiane 0x00000004
#define dageograagegr 0x00000008
#define daCSafila 0x00000100


#define ddd_mm_ss 0x00000010
#define ddd_dddd  0x00000020
/* classe generale per la trasformazione di coordinate e datum*/

struct _POINT
{
  double estcs;
  double nordcs;
  double locs;
  double fics;
  double logb;
  double figb;
  double estgbFE;
  double nordgbFE;
  double estgbFO;
  double nordgbFO;
  double lo2000;
  double fi2000;
  double estUTM;
  double nordUTM;
  int fuso;
};

typedef  _POINT _point;
#define o_u_s  0x00000000 //uscito in sessaggesimali
#define o_u_r  0x00000001 //uscita in radianti
#define o_u_c  0x00000002 //uscita in centesimali
#define o_u_d  0x00000003 //uscita in decimali
#define o_u_p  0x00000004 //uscita in coordinate piane


class dacsagb
{
  public:
  double EstCS;
  double NordCS;
  double fiCS;
  double laCS;
  double Est_festGB;
  double Est_fovestGB;
  double Nord_festGB;
  double Nord_fovestGB;
  double fiGB;
  double laGB;
  string messaggio;
  string nomefilecxf;
  string nomefilelog;
  string  nomefileout;
  string  cartellageo;
  string file_gsb; //file con le correzioni da roma40 a etrf2000
  bool ses;
  bool rad;
  bool deg;
  bool cent;
  bool piane;

  int op;
  /*
  * op specifica le operazioni da eseguire
  */
  dacsagb() {
    EstCS=0e0;
    NordCS=0e0;
    fiCS=0e0;
    laCS=0e0;
    Est_festGB=0e0;
    Est_fovestGB=0e0;
    Nord_festGB=0e0;
    Nord_fovestGB=0e0;
    fiGB=0e0;
    laGB=0e0;
    op = 0;
    nomefilecxf.clear();
    nomefilelog.clear();
    nomefileout.clear();
    cartellageo.clear();
    file_gsb.clear();
    ses = false;
    rad = false;
    deg = false;
    cent = false;
    piane = true;
  }
public:
  dacsagb(double par1, double par2, int par3);

  bool calcolaCXFC(string filemp, string fileout, string filelog,string cartellagbs, int fmt);
  bool calcolaCXF(char* filemp, char* fileout, char* filelog, char* cartellagbs, int fmt);
  

};

#define FUSO_OVEST 0x00000001
#define FUSO_EST 0x00000002

bool DaPianeCSaFiLa(double est, double nord, double& la, double& fi, string & ms);
void calcolo(double &estCS, double& nordCS, double& laCS, double& fiCS,double& estGBFE, double& nordGBFE, double& estGBFO, double& nordGBFO, double& laGB, double& fiGB);
void calcolo(_POINT &p ,bool utm=false);
//int  est_nordCS_est_nordGB(double estCS, double nordCS, int fuso_richiesto,double& estGB, double& nordGB);
int  la_fiCS_la_fiGB(double laCS, double fiCS, int u, double& laGB, double& fiGB);
int  la_fiCS_est_nordCS(double laCS, double fiCS, int u, double& estCS, double& nordCS);
int  la_fiGB_est_nordGB(double laGB, double fiGB, int u, double& estGB, double& nordGB);
int  est_nordCS_la_fiCS(double estCS, double nordCS, double& laCS, double& fiCS);
int  est_nordGB_la_fiGB(double estGB, double nordGB, double& laGB, double& fiGB);



string removfirstblanck(string a);

bool calcolaunpunto(GRI_HDR* hh, double nordcs, double estcs, double& nordutm, double& estutm, int& fuso);

extern string nomefilegriglia;

