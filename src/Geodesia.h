// Geodesia.h: interface for the Geodesia class.
//
//////////////////////////////////////////////////////////////////////
#pragma once
#if !defined(AFX_GEODESIA_H__1A14C544_6433_11D3_84F6_0000B497AE58__INCLUDED_)
#define AFX_GEODESIA_H__1A14C544_6433_11D3_84F6_0000B497AE58__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#define awgs84 6378137.0e0
#define e2wgs84 0.006694380e0

#define ahi  6378388.0e0
#define bhi  6356911.946
#define e2hi (ahi* ahi - bhi * bhi) / ahi / ahi
#define i_u_s  0x00000000 //ingresso in sessaggesimali
#define i_u_r  0x00000001 //ingresso in radianti
#define i_u_c  0x00000002 //ingresso in centesimali
#define i_u_d  0x00000003 //ingresso in decimali




class Geodesia 
{

public:
  double ang_conv(double ang, int un);
	double fis(double s,double alfa, int un = i_u_s);  //trasporto latitudine
  double las(double s, double alfa, int un = i_u_s);  //trasporto longitudine
  void fi_la(double s, double alfa, double& la, double& lo,int un = i_u_s);//s positivo o negativo alfa positivo
  void fi_la_step(double s, double alfa, double& la, double& lo, int un = i_u_s);//s positivo o negativo alfa positivo

  void s_alfa(double fi0, double la0, double fi1,double la1, double &s,double &az1,double &az2, int un = i_u_r);//s positivo o negativo alfa positivo
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
    void SetAsse(double x=ahi){semiasse=x;} ;  /* setta semiasse                        */
    void SetEcc(double x=e2hi){ecc=x;}       ;  /* setta eccentricita'                   */
    void SetEllWgs84(){SetAsse(awgs84);SetEcc(e2wgs84);}
	  void convFiLa_da_wgs84_a_h(double f,double l,double q=0);
	  void convFiLa_da_h_a_wgs84(double f,double l,double q=0);
    void calcoladaXYZ(double xx, double yy, double zz);
	Geodesia(); //inizializza tutti i valori sul default
  void calcola(int un=i_u_s,double fii=41.552551,double lamd=12.270840,double q=0.0e0); //equivale a setta fi,lambda
  double solvefi(double x,double y,double z,double f0);
  double xx(); //coordinate cartesiane del punto sull'ellisoide
  double yy();
  double zz();
	double xxq(); //coordinate cartesiane del punto in quota
	double yyq();
	double zzq();
  double fi; //sempre in radianti
  double lambda; //sempre in radianti
  double W;
  double RG;
  double GN;
  double ro;
  double r;
  double x,y,z; //cordinate geocentriche correnti
               //calcolate esplicitamente chiamando xx,yy,zz 
  double xq,yq,zq; //cordinate geocentriche correnti
                //calcolate esplicitamente chiamando xq,yq,zq 

  
  double NordFE,EstFE;   //coordinate Gauss-Boaga su ellissoide Hayford fuso est
  double NordFO,EstFO;   //coordinate Gauss-Boaga su ellissoide Hayford fuso oves
  double crid;           //coefficiente di contrazione 
  double lfi(); //menbro integrale ellittico
	void NEgb(double f,double l,bool MM=true); //Nord Est Gauss-Boaga  l relativa a MomteMario se MM=true
	void FiLagb(double N,double E, double & fi ,double & la ); //calcola fi e lamda hayford da nORD ED eST gb
  void DaCSaWgs84(double n,double e,double q,double * xw,double * yw,double * zw);                                       //                      l relativa a Greenwich se MM=FALSE     
  void DaGBaWgs84(double n,double e,double q,double * xw,double * yw,double * zw);                                       //                      l relativa a Greenwich se MM=FALSE     
  double xcentrohay,ycentrohay,zcentrohay; //cordinate del centro di hayford in wgs84
  double sec(double x);
  double cosec(double x);
  double lat_geoc();
  double lat_ridotta(double lat);
	//virtual ~Geodesia();

};

#endif // !defined(AFX_GEODESIA_H__1A14C544_6433_11D3_84F6_0000B497AE58__INCLUDED_)



