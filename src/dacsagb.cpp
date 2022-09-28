
#include "interpola.h"
#include "dacsagb.h"
using namespace std;


#include <limits.h>
#include <cerrno>
//ifstream impfilecxf;
ofstream outfile;
ofstream logfile;

string nomefilegriglia="";
double PI = (double)3.14159265358979323846264338327950; /* pigrego */
double RGC = (double)(200e0 / PI); /* coeff da rad a GC */
double ARCS = (PI / 180e0) / 60e0 / 60e0;

double latitudine=VALORE_NUMERICO_NULLO;
double longitudine= VALORE_NUMERICO_NULLO;
double dasag(double ang) //da sessaggesimali a centesimali
{
  double ss, gg, mm, ret, angb;
  angb = ang;
  if (ang < 0)
  {
    ang = -angb;
  }

  gg = floor(ang);
  mm = floor((ang - gg) * 100e0);
  ss = (ang * 100e0 - floor(ang * 100e0)) * 100e0;
  ret = gg;
  ret += mm / 60e0;
  ret += ss / 3600e0;
  ret /= 0.9e0;
  if (angb < 0)
  {
    ret = -ret;
  }
  return ret;
}

double dagas(double ang) //da  centesimali a sessaggesimali
{
  double ss, gg, mm, dag, angb;
  angb = ang;
  if (ang < 0)
  {
    ang = -angb;
  }
  dag = ang * 0.9e0;
  gg = floor(dag);
  ss = (dag - gg) * 3600e0;
  mm = floor(ss / 60e0);
  ss = ss - mm * 60e0;
  dag = gg + mm / 1.0e2 + ss / 1.0e4;
  if (angb < 0)
  {
    dag = -dag;
  }
  return dag;
}

double dasar(double ang) //da sessaggesimali a radianti
{
  double d = dasag(ang);
  return d / RGC;
}
double dagar(double ang) //da centesimali a radianti
{
  double d = ang / RGC;
  return d;
}
double daras(double ang) //da radianti a sessagesimali
{
  double d = ang * RGC;
  d = dagas(d);
  return d;

}
double darag(double ang) //da radianti a centesimali
{
  double d = ang * RGC;
  return d;
}



double dasdas(double ang) //da sessadecimali a sessaggesimali
{
  double ss, gg, mm, dag, angb;
  angb = ang;
  if (ang < 0)
  {
    ang = -angb;
  }
  dag = ang;
  gg = floor(dag);
  ss = (dag - gg) * 3600e0;
  mm = floor(ss / 60e0);
  ss = ss - mm * 60e0;
  dag = gg + mm / 1.0e2 + ss / 1.0e4;
  if (angb < 0)
  {
    dag = -dag;
  }
  return dag;
}
double dasasd(double ang) //da sessaggesimali a sessadecimali 
{
  double ss, gg, mm, ret, angb;
  angb = ang;
  if (ang < 0)
  {
    ang = -angb;
  }

  gg = floor(ang);
  mm = floor((ang - gg) * 100e0);
  ss = (ang * 100e0 - floor(ang * 100e0)) * 100e0;
  ret = gg;
  ret += mm / 60e0;
  ret += ss / 3600e0;
  if (angb < 0)
  {
    ret = -ret;
  }
  return ret;
}


double adjang400(double ang)
{
  do
  {
    if (ang < 0e0F) ang = ang + 400e0F;
    if (ang >= 400e0F) ang -= 400e0F;
  } while (ang < 0e0F && ang >= 400e0F);
  return ang;
}




void angdir(double xs, double ys, double xt, double yt, double& dz, double& teta, double& cd, double& sd)
{
  double dx, dy, dz2, de;
  dx = xt - xs;
  dy = yt - ys;
  if (dx == 0e0 && dy == 0e0)
  {
    dz = 0e0;
    cd = 0e0;
    sd = 0e0;
    teta = 0;
    return;
  }
  dz2 = dx * dx + dy * dy;
  dz = sqrt(dz2);
  de = dy + dz;
  teta = PI;
  if (de != 0e0)teta = 2 * atan(dx / de);
  if (dz >= 0e0)
  {
    cd = cos(teta) / (dz);
    sd = sin(teta) / (dz);
  }
  else
  {
    cd = 0e0;
    sd = 0e0;
    teta = 0e0;
  }
  if (teta < 0e0) teta = teta + 2 * PI;
}


double Geodesia::ang_conv(double ang, int un)
{
  double rang = ang;
  switch (un)
  {
  case  i_u_s:
  {
    if (ang > 0)
      rang = dasar(ang);
    else
      rang = -dasar(fabs(ang));
  }
  break;
  case  i_u_c:
  {
    rang = dagar(ang);
  }
  break;
  case  i_u_d:
  {
    rang = (ang)*PI / 180e0;
  }
  break;
  case  i_u_r:
  {
    rang = ang;
  }
  break;

  default:
    break;
  }
  return rang;
}

Geodesia::Geodesia()
{
  //Setta i Default sull'ellissoide internazionale
  SetAsse();
  SetEcc();
  calcola();
  crid = 0.9996e0;
  //   xcentrohay=-224.086e0;
  //   ycentrohay=-66.992e0;
  //   zcentrohay=12.934e0;
  xcentrohay = -220.525e0;
  ycentrohay = -66.206e0;
  zcentrohay = 12.934e0;
  //
}


void Geodesia::calcola(int un, double fii, double lamd, double q)//equivale a setta fi,_lambda
{
  quota = q;
  fi = ang_conv(fii, un);
  _lambda = ang_conv(lamd, un);
  W = ecc * pow(sin(fi), 2e0);
  W = sqrt(1e0 - W);
  double cs = cos(fi);
  r = semiasse * cs / (W);
  ro = semiasse;
  ro *= (1e0 - ecc);
  ro /= pow(W, 3e0);
  GN = semiasse / (W);
  RG = sqrt(ro * GN);
  x = xx();
  y = yy();
  z = zz();
  xq = xxq();
  yq = yyq();
  zq = zzq();
  //  FILE *dummy=fopen("pregeo8.dmm","wt");
  //  fprintf(dummy,"%f\t%f\t%f\t%f\t%f\t%f\t",fi,W,r,ro,GN,RG); //carica il f.p support;
  //  fclose(dummy);
  //  remove("pregeo8.dmm");
}


void Geodesia::calcoladaXYZ(double xxi, double yyi, double zzi)
{
  x = xxi;
  y = yyi;
  z = zzi;
  _lambda = atan2(y, x);
  double fi0 = asin(sqrt((pow(semiasse, 2) - pow(x, 2) - pow(y, 2)) /
    (pow(semiasse, 2) - ecc * (pow(x, 2) + pow(y, 2)))));
  fi = solvefi(x, y, z, fi0);

  calcola(i_u_r, (fi), (_lambda));

  double qu = sqrt(pow(xxi - x, 2) + pow(yyi - y, 2) +
    pow(zzi - z, 2));

  calcola(i_u_r, (fi), (_lambda), qu);

}





double Geodesia::xx()
{
  x = semiasse * cos(fi) * cos(_lambda) / W;
  return x;
}
double Geodesia::yy()
{
  y = semiasse * cos(fi) * sin(_lambda) / W;
  return y;
}
double Geodesia::zz()
{
  z = semiasse * (1e0 - ecc) * sin(fi) / W;
  return z;
}

#define TT(f)      sqrt(1-ecc*pow(sin(f),2))
#define T(f0,xcorr,ycorr,zcorr) sqrt(xcorr*xcorr+ycorr*ycorr)*sin(f0)/cos(f0)-semiasse*ecc*sin(f0)/TT(f0)-zcorr


double Geodesia::solvefi(double xcorr, double ycorr, double zcorr, double f0)
{
  double f, tfa, tfb;
  int ciclo = 0;
  while (1 == 1)
  {
    tfa = T(f0, xcorr, ycorr, zcorr);
    f =
      (
        -pow
        (
          (
            1 - ecc * pow(sin(f0), 2)
            ), (3 / 2)
        ) * cos(f0) *
        (
          (
            zcorr * sqrt
            (
              1 - ecc * pow(sin(f0), 2)
            ) + semiasse * ecc * sin(f0)
            ) * cos(f0) - sqrt
            (
              pow(xcorr, 2) + pow(ycorr, 2)
            ) * sin(f0) * sqrt
            (
              1 - ecc * pow(sin(f0), 2)
            )
          ) /
        (
          (
            semiasse * ecc * pow(cos(f0), 3) - sqrt
            (
              pow(xcorr, 2) + pow(ycorr, 2)
            ) * pow
            (
              (
                1 - ecc * pow(sin(f0), 2)
                ), (3 / 2)
            )
            ) * sqrt
            (
              1 - ecc * pow(sin(f0), 2)
            )
          )
        );
    f0 = f0 + f;
    tfb = T(f0, xcorr, ycorr, zcorr);
    if (tfa == tfb || ciclo++ == 50) break;
  }
  return f0;
}

void Geodesia::NEgb(double f, double l, bool MM)
{
  SetAsse();
  SetEcc();
  calcola();
  double lla = fabs(l);
  double ll;
  if (MM)
  {
    if (l > 0)
      ll = _lambda + dasag(lla) / RGC;
    else
      ll = _lambda - dasag(lla) / RGC;
  }
  else
  {
    ll = dasag(lla) / RGC;
  }
  double lo = ll - dasag(9.0000) / RGC;
  double le = ll - dasag(15.0000) / RGC;
  ll *= RGC;
  ll = dagas(ll);
  l = ll;
  lo *= RGC;
  le *= RGC;
  if (lo > 0)
    lo = dagas(lo);
  else
    lo = -dagas(fabs(lo));
  if (le > 0)
    le = dagas(le);
  else
    le = -dagas(fabs(le));

  //1) calcolo per il fuso ovest
  calcola(i_u_s, f, lo); //setta tutte le grandezze
  double lf = lfi();
  NordFO = lf;
  double t = tan(fi);
  double eta = (GN - ro) / ro;
  NordFO += pow(_lambda, 2) * (GN * sin(fi) * cos(fi)) / 2;
  NordFO += pow(_lambda, 4) * (GN * sin(fi) * pow(cos(fi), 3)) * (5 - t * t + 9 * eta + 4 * eta * eta) / 24;
  NordFO += pow(_lambda, 6) * (GN * sin(fi) * pow(cos(fi), 5)) * (61 - 58 * t * t + pow(t, 4)) / 720;
  NordFO *= crid;
  EstFO = _lambda * GN * cos(fi);
  EstFO += pow(_lambda, 3) * GN * pow(cos(fi), 3) * (1 - t * t + eta) / 6;
  EstFO += pow(_lambda, 5) * GN * pow(cos(fi), 5) * (5 - 18 * t * t + pow(t, 4)) / 120;
  EstFO *= crid;
  EstFO += 1500000.0;
  //1) calcolo per il fuso est
  calcola(i_u_s, f, le); //setta tutte le grandezze
  NordFE = lf;
  t = tan(fi);
  eta = (GN - ro) / ro;
  NordFE += pow(_lambda, 2) * (GN * sin(fi) * cos(fi)) / 2;
  NordFE += pow(_lambda, 4) * (GN * sin(fi) * pow(cos(fi), 3)) * (5 - t * t + 9 * eta + 4 * eta * eta) / 24;
  NordFE += pow(_lambda, 6) * (GN * sin(fi) * pow(cos(fi), 5)) * (61 - 58 * t * t + pow(t, 4)) / 720;
  NordFE *= crid;
  EstFE = _lambda * GN * cos(fi);
  EstFE += pow(_lambda, 3) * GN * pow(cos(fi), 3) * (1 - t * t + eta) / 6;
  EstFE += pow(_lambda, 5) * GN * pow(cos(fi), 5) * (5 - 18 * t * t + pow(t, 4)) / 120;
  EstFE *= crid;
  EstFE += 2520000.0;
}

void Geodesia::NEutm(double f, double l, int & fuso)// angoli in sessagesimali
{
  SetEllWgs84();
  double lla = fabs(l);
  double ll =dasag(lla) / RGC;
  //calcolo del fuso
  double ilf = l;
  double mer_centr = 0e0;
  if (6 < ilf && ilf <= 12)
  {
    fuso = 32;
    mer_centr = 9.0;
  }
  if (12 < ilf && ilf <= 18)
  {
    fuso = 33;
    mer_centr = 15.0;
  }
  if (18 < ilf && ilf <= 24)
  {
    fuso = 34;
    mer_centr = 21.0;
  }


  double lo = ll - dasag(mer_centr) / RGC;
  ll *= RGC;
  ll = dagas(ll);
  l = ll;
  lo *= RGC;
 
  if (lo > 0)
    lo = dagas(lo);
  else
    lo = -dagas(fabs(lo));
 

  //1) calcolo 
  calcola(i_u_s, f, lo); //setta tutte le grandezze
  double lf = lfi();
  NordUTM = lf;
  double t = tan(fi);
  double eta = (GN - ro) / ro;
  NordUTM += pow(_lambda, 2) * (GN * sin(fi) * cos(fi)) / 2;
  NordUTM += pow(_lambda, 4) * (GN * sin(fi) * pow(cos(fi), 3)) * (5 - t * t + 9 * eta + 4 * eta * eta) / 24;
  NordUTM += pow(_lambda, 6) * (GN * sin(fi) * pow(cos(fi), 5)) * (61 - 58 * t * t + pow(t, 4)) / 720;
  NordUTM *= crid;
  EstUTM = _lambda * GN * cos(fi);
  EstUTM += pow(_lambda, 3) * GN * pow(cos(fi), 3) * (1 - t * t + eta) / 6;
  EstUTM += pow(_lambda, 5) * GN * pow(cos(fi), 5) * (5 - 18 * t * t + pow(t, 4)) / 120;
  EstUTM *= crid;
  EstUTM += 500000.0;
}


double Geodesia::lfi() //integrazione arco di meridiano
{
  double e4 = ecc * ecc;
  double e6 = e4 * ecc;
  double e8 = e6 * ecc;
  double e10 = e8 * ecc;
  double A = 1 + (3.0 / 4) * ecc;
  A += (45.0 / 64) * e4;
  A += (175.0 / 256) * e6;
  A += (11025.0 / 16384) * e8;
  A += (43659.0 / 65536) * e10;
  double B = (3.0 / 4) * ecc;
  B += (15.0 / 16) * e4;
  B += (525.0 / 512) * e6;
  B += (2205.0 / 2048) * e8;
  B += (72765.0 / 65536) * e10;
  double C = (15.0 / 64) * e4;
  C += (105.0 / 256) * e6;
  C += (2205.0 / 4096) * e8;
  C += (10395.0 / 16384) * e10;
  double D = (35.0 / 512) * e6;
  D += (315.0 / 2048) * e8;
  D += (31185.0 / 131072) * e10;
  double E = (315.0 / 16384) * e8;
  E += (3465.0 / 65536) * e10;
  double F = (639.0 / 131072) * e10;
  double alfa = A * (1.0 - ecc) * semiasse;
  double beta = B * (1.0 - ecc) * semiasse / 2;
  double gamma = C * (1.0 - ecc) * semiasse / 4;
  double delta = D * (1.0 - ecc) * semiasse / 6;
  double epsilon = E * (1.0 - ecc) * semiasse / 8;
  double cseta = F * (1.0 - ecc) * semiasse / 10;
  double rt = alfa * fi;
  rt -= beta * sin(2 * fi);
  rt += gamma * sin(4 * fi);
  rt -= delta * sin(6 * fi);
  rt += epsilon * sin(8 * fi);
  rt -= cseta * sin(10 * fi);
  return rt;
}

double Geodesia::xxq()
{
  // xq=x+quota*cos(fi)*cos(_lambda)/W;
  xq = x + quota * cos(fi) * cos(_lambda);
  return xq;

}
double Geodesia::yyq()
{
  //  yq=y+quota*cos(fi)*sin(_lambda)/W;
  yq = y + quota * cos(fi) * sin(_lambda);
  return yq;

}
double Geodesia::zzq()
{
  //  zq=z+quota*(1e0-ecc)*sin(fi)/W;
  zq = z + quota * sin(fi);
  return zq;

}

void Geodesia::convFiLa_da_wgs84_a_h(double f, double l, double q)
{
  SetEllWgs84();      //esegue il calcolo di x,y,z in wgs84
  calcola(i_u_s, f, l, q);
  double xh = xq - xcentrohay;
  double yh = yq - ycentrohay;
  double zh = zq - zcentrohay;
  SetAsse(ahi);
  SetEcc(e2hi);
  double fi0 = asin(sqrt((pow(semiasse, 2) - pow(xh, 2) - pow(yh, 2)) /
    (pow(semiasse, 2) - ecc * (pow(xh, 2) + pow(yh, 2)))));
  fi = solvefi(xh, yh, zh, fi0);
  fi = dagas(fi * RGC);
  _lambda = atan2(yh, xh);
  _lambda = dagas(_lambda * RGC);
  calcola(i_u_s, fi, _lambda);
  quota = sqrt((xh - x) * (xh - x) + (yh - y) * (yh - y) + (zh - z) * (zh - z));
  if (xh < x && yh < y && zh < z)
  {
    quota = -quota;

  }
  // fi=dagas(fi*RGC);
  // _lambda=dagas(_lambda*RGC);
}
void Geodesia::convFiLa_da_h_a_wgs84(double f, double l, double q)
{
  SetAsse(ahi);
  SetEcc(e2hi);
  calcola(i_u_s, f, l, q);
  double xh = xq + xcentrohay;
  double yh = yq + ycentrohay;
  double zh = zq + zcentrohay;
  SetEllWgs84();      //esegue il calcolo di x,y,z in wgs84
  double fi0 = asin(sqrt((pow(semiasse, 2) - pow(xh, 2) - pow(yh, 2)) /
    (pow(semiasse, 2) - ecc * (pow(xh, 2) + pow(yh, 2)))));
  fi = solvefi(xh, yh, zh, fi0);
  //  fi=dagas(fi*RGC);
  _lambda = atan2(yh, xh);
  //  _lambda=dagas(_lambda*RGC);
  xq = xh;
  yq = yh;
  zq = zh;
}


void Geodesia::DaCSaWgs84(double n, double e, double q, double* xw, double* yw, double* zw)
{
  double actualfi = dagas(fi * RGC);
  double actualla = dagas(_lambda * RGC);
  double actualasse = semiasse;
  double actualecc = ecc;
  SetAsse();  /*setta semiasse hayford                */
  SetEcc();   /*setta eccentricita'hayford            */
  calcola();  /*setta fi,lamda di M.mario             */

  //calcola s ed alfa
  double epsilon = (n * e) / (2 * ro * GN);
  epsilon /= 3;
  double alfa = atan2(e + epsilon * n, n - epsilon * e * 2);
  double s = pow(e + epsilon * n, 2);
  s += pow(n - epsilon * e * 2, 2);
  s = sqrt(s);
  //controllo
  // trasporto coordinate geografiche
  double fip = fis(s, alfa);
  fip = dagas(fip * RGC);
  double lap = las(s, alfa);
  lap = dagas(lap * RGC);
  double epsilon1 = s * s * sin(alfa) * cos(alfa) / (6 * ro * GN);
  double xie = s * sin(alfa - epsilon1);
  double yie = s * cos(alfa - 2 * epsilon1);
 // double d = xie - e;
//  d = yie - n;
  double gn2 = GN;
  calcola();
  xie = (dasar(lap) - _lambda) * gn2 * cos(dasar(fip));
  double fi3 = dasar(fip) + xie * xie * tan(dasar(fip)) / (2 * ro * GN);
  yie = ro;
  yie *= (fi3 - fi);
  yie *= (1 + (3 * sin(2 * fi) * ecc) * (fi3 - fi) / (4 * (1 - ecc * pow(sin(fi), 2))));
  calcola(i_u_s, fip, lap, q);
  convFiLa_da_h_a_wgs84(fip, lap, q);
  *xw = xq;
  *yw = yq;
  *zw = zq;
  SetAsse(actualasse);  /*setta semiasse hayford                */
  SetEcc(actualecc);   /*setta eccentricita'hayford            */
  calcola(i_u_s, actualfi, actualla);  /*resetta fi,lamda              */
}

void Geodesia::DaGBaWgs84(double n, double e, double q, double* xw, double* yw, double* zw)
{
  double actualfi = dagas(fi * RGC);
  double actualla = dagas(_lambda * RGC);
  double actualasse = semiasse;
  double actualecc = ecc;
  SetAsse();  /*setta semiasse hayford                */
  SetEcc();   /*setta eccentricita'hayford            */
  calcola();  /*setta fi,lamda di M.mario             */
  double fip, lap;
  FiLagb(n, e, fip, lap);
  fip = dagas(fip * RGC);
  lap = dagas(lap * RGC);

  calcola(i_u_s, fip, lap, q);
  convFiLa_da_h_a_wgs84(fip, lap, q);
  *xw = xq;
  *yw = yq;
  *zw = zq;
  SetAsse(actualasse);  /*setta semiasse hayford                */
  SetEcc(actualecc);   /*setta eccentricita'hayford            */
  calcola(i_u_s, actualfi, actualla);  /*resetta fi,lamda              */
}

double Geodesia::fis(double s, double alfa, int un)
{
  alfa = ang_conv(alfa, un);
  if (s < 0e0)
  {
    alfa += PI;
    s = -s;
    if (alfa > 2 * PI) alfa += 2 * PI;
  }
  double fff = 0e0;
  double a = semiasse;
  double sinf = sin(fi);
  double sinf2 = sinf * sinf;
  double sin2f = sin(2e0 * fi);
  double sin2f2 = sin2f * sin2f;
  double cosf = cos(fi);
  double cosf2 = cosf * cosf;
  double tanf = tan(fi);
  double tanf2 = tanf * tanf;
//  double tanf3 = pow(tanf, 3);
  double GN2 = GN * GN;
  double a2 = a * a;
 // double a3 = pow(a, 3);
  double r2 = r * r;
  double sinalfa = sin(alfa);
  double sinalfa2 = pow(sinalfa, 2);
  double cosalfa = cos(alfa);
  double cosalfa2 = pow(cosalfa, 2);
  double e = ecc;
  double W2 = W * W;
  double W3 = W2 * W;
  double W9 = pow(W, 9);
  double W10 = pow(W, 10);
  double W12 = pow(W, 12);
  double W13 = pow(W, 13);
  double W15 = pow(W, 15);

  double t = (3 * tanf2 + 1);
  double e12 = (1 - ecc);




  double t1 = fi + s * cosalfa / ro;
  double t2 = pow(s, 2) * sinf / (2 * ro);
  t2 *= sinalfa2 / (GN * cosf) + (3 * e * cosf * cosalfa2) / (ro * W2);
  double t3 = pow(s, 3) * sinalfa2 * cosalfa / (6 * pow(ro, 3) * (1 + 3 * tanf2));

  double t4 = (9 * e * cosalfa2 * sinalfa2 * cosf * sinf * W10 * t) / (pow((e12 * a), 4));
  t4 += (pow(sinalfa, 4) * sinf * W9 * t) / (pow((e12 * a), 3) * r);
  t4 -= (2 * cosalfa2 * sinalfa2 * sinf * W9 * t) / (pow((e12 * a), 3) * r);
  t4 -= (6 * cosalfa2 * sinalfa2 * W12 * tanf) / (pow(e12 * a, 4) * cosf2);
  t4 *= pow(s, 4) / 24.0e0;

  double t5 = -12 * pow(cosalfa, 3) * sinalfa2 * (1 / cosf2) * W15 * tanf2 / pow((1 - e) * a, 5);
  t5 += (126 * e * pow(cosalfa, 3) * sinalfa2 * cosf * (1 / cosf2) * sinf * W13 * tanf) / pow((1 - e) * a, 5);
  t5 += (18 * cosalfa * pow(sinalfa, 4) * (1 / cosf2) * sinf * W12 * tanf) / (pow((1 - e) * a, 4) * r);
  t5 -= (24 * pow(cosalfa, 3) * sinalfa2 * (1 / cosf2) * sinf * W12 * tanf) / (pow((1 - e) * a, 4) * r);
  t5 -= (6 * pow(cosalfa, 3) * sinalfa2 * (1 / (cosf2 * cosf2)) * W15) / pow((1 - e) * a, 5);
  t5 -= (9 * e * t * pow(cosalfa, 3) * sinalfa2 * sinf2 * W13) / pow((1 - e) * a, 5);
  t5 += (9 * e * t * pow(cosalfa, 3) * sinalfa2 * cosf2 * W13) / pow((1 - e) * a, 5);
  t5 += (t * cosalfa * pow(sinalfa, 4) * cosf * W12) / (pow((1 - e) * a, 4) * r);
  t5 -= (2 * t * pow(cosalfa, 3) * sinalfa2 * cosf * W12) / (pow((1 - e) * a, 4) * r);
  t5 -= (90 * e * e * t * pow(cosalfa, 3) * sinalfa2 * cosf2 * sinf2 * pow(W, 11)) / pow((1 - e) * a, 5);
  t5 -= (27 * e * t * cosalfa * pow(sinalfa, 4) * cosf * sinf2 * pow(1 - e * sinf2, 5)) / (pow((1 - e) * a, 4) * r);
  t5 += (36 * e * t * pow(cosalfa, 3) * sinalfa2 * cosf * sinf2 * pow(1 - e * sinf2, 5)) / (pow((1 - e) * a, 4) * r);
  t5 += (9 * t * cosalfa * pow(sinalfa, 4) * sinf2 * W9) / (pow((1 - e) * a, 3) * r2);
  t5 -= (6 * t * pow(cosalfa, 3) * sinalfa2 * sinf2 * W9) / (pow((1 - e) * a, 3) * r2);

  t5 *= pow(s, 5) / 120e0;

  fff = t1 - t2 - t3 + t4 + t5;
  double p = s * cos(alfa) / ro;
  double q = s * sin(alfa) / r;



  double ffc = fi + p;
  ffc -= 3 * ecc * GN2 * sin2f / (4 * a2) * pow(p, 2);
  ffc -= GN * sin2f / (4 * ro) * pow(q, 2);
  ffc += (ecc * GN2 / (2 * a2) * (ecc * GN2 * sin2f2) / (a2)-cos(2 * fi)) * pow(p, 3);
  ffc += (1 / 6) * ((r * r * GN / (ro * a2) * (10 * ecc * sinf2 - 1) - 3 * sinf2)) * p * pow(q, 2);
 // double diff = (fff - ffc) / ARCS;
  //nuova serie


  ;
  //sostituzioni;;
  double ca = cos(alfa);
  double sa = sin(alfa);
  double cf = cos(fi);
  double cf2 = cf * cf;
  double cf3 = cf2 * cf;
  double cf4 = cf3 * cf;
  double sf = sin(fi);
  double sf2 = pow(sf, 2);
  double sf3 = sf2 * sf;
  double sf4 = sf3 * sf;
  double e2 = e * e;
  double e3 = e2 * e;
  double e4 = e3 * e;
  double W5 = pow(W, 5);

  //derivata 1 di f in dx variabile df_dx;
  double df_dx = ca / ro;

  //derivata 1 di a in dx variabile da_dx;
  double da_dx = sa * sf / r;
  ;
  //derivata 2 di f in dx;
  ;
  ;
  double d2f_dx2 = (-(3 * e * ca * cf * sf * W * df_dx) / (e12 * a)) - (sa * da_dx * W3) / (e12 * a);
  ;
  ;
  //double d2f_dx2_p = -(sinf / ro) * (sinalfa2 / (GN * cosf) + (3 * e * cosf * cosalfa2) / (ro * W2));
  //derivata 2 di a in dx variabile d2a_dx2;
  ;
  ;
  double d2a_dx2 = (sa * sf2 * W * df_dx) / (a * cf2);
  d2a_dx2 += (sa * W * df_dx) / a;
  d2a_dx2 -= (e * sa * sf2 * df_dx) / (a * W);
  d2a_dx2 += (ca * da_dx * sf * W) / (a * cf);
  ;
  //derivata 3 di f in dx;
  ;
  ;
  double d3f_dx3 = (-(3 * e * ca * cf * sf * W * d2f_dx2) / (e12 * a));
  d3f_dx3 += (3 * e * ca * sf2 * W * pow(df_dx, 2)) / (e12 * a);
  d3f_dx3 -= (3 * e * ca * cf2 * W * pow(df_dx, 2)) / (e12 * a);
  d3f_dx3 += (3 * e * ca * cf2 * sf2 * pow(df_dx, 2)) / (e12 * a * W);
  d3f_dx3 += (6 * e * sa * da_dx * cf * sf * W * df_dx) / (e12 * a);
  d3f_dx3 -= (sa * d2a_dx2 * W3) / (e12 * a);
  d3f_dx3 -= (ca * pow(da_dx, 2) * W3) / (e12 * a);
  ;
  ;
  ;
  ;
  ;
  //derivata 3 di a in dx variabile d3a_dx3;
  ;
  ;
  double d3a_dx3 = (sa * sf2 * W * d2f_dx2) / (a * cf2);
  d3a_dx3 += (sa * W * d2f_dx2) / a - (e * sa * sf2 * d2f_dx2) / (a * W);
  d3a_dx3 += (2 * sa * sf3 * W * pow(df_dx, 2)) / (a * cf3);
  d3a_dx3 += (2 * sa * sf * W * pow(df_dx, 2)) / (a * cf);
  d3a_dx3 -= (e * sa * sf3 * pow(df_dx, 2)) / (a * cf * W);
  d3a_dx3 -= (3 * e * sa * cf * sf * pow(df_dx, 2)) / (a * W);
  d3a_dx3 -= (e * sa * cf * sf3 * W3 * pow(df_dx, 2)) / a;
  d3a_dx3 += (2 * ca * da_dx * sf2 * W * df_dx) / (a * cf2);
  d3a_dx3 += (2 * ca * da_dx * W * df_dx) / a;
  d3a_dx3 -= (2 * e * ca * da_dx * sf2 * df_dx) / (a * W);
  d3a_dx3 += (ca * d2a_dx2 * sf * W) / (a * cf);
  d3a_dx3 -= (sa * pow(da_dx, 2) * sf * W) / (a * cf);
  ;
  ;
  ;
  //derivata 4 di f in dx  variaqbile d4f_dx4;
  ;
  ;
  double d4f_dx4 = (-(3 * e * ca * cf * sf * W * d3f_dx3) / (e12 * a));
  d4f_dx4 += (9 * e * ca * sf2 * W * df_dx * d2f_dx2) / (e12 * a);
  d4f_dx4 -= (9 * e * ca * cf2 * W * df_dx * d2f_dx2) / (e12 * a);
  d4f_dx4 += (9 * e * ca * cf2 * sf2 * df_dx * d2f_dx2) / (e12 * a * W);
  d4f_dx4 += (9 * e * sa * da_dx * cf * sf * W * d2f_dx2) / (e12 * a);
  d4f_dx4 += (12 * e * ca * cf * sf * W * pow(df_dx, 3)) / (e12 * a);
  d4f_dx4 -= (9 * e * ca * cf * sf3 * pow(df_dx, 3)) / (e12 * a * W);
  d4f_dx4 += (9 * e * ca * cf3 * sf * pow(df_dx, 3)) / (e12 * a * W);
  d4f_dx4 += (3 * e3 * ca * cf3 * sf3 * W3) * pow(df_dx, 3) / (e12 * a);
  d4f_dx4 -= (9 * e * sa * da_dx * sf2 * W * pow(df_dx, 2)) / (e12 * a);
  d4f_dx4 += (9 * e * sa * da_dx * cf2 * W * pow(df_dx, 2)) / (e12 * a);
  d4f_dx4 -= (9 * e * sa * da_dx * cf2 * sf2 * pow(df_dx, 2)) / (e12 * a * W);
  d4f_dx4 += (9 * e * sa * d2a_dx2 * cf * sf * W * df_dx) / (e12 * a);
  d4f_dx4 += (9 * e * ca * pow(da_dx, 2) * cf * sf * W * df_dx) / (e12 * a);
  d4f_dx4 -= (sa * d3a_dx3 * W3) / (e12 * a);
  d4f_dx4 -= (3 * ca * da_dx * d2a_dx2 * W3) / (e12 * a);
  d4f_dx4 += (sa * pow(da_dx, 3) * W3) / (e12 * a);
  ;
  ;
  ;
  ;
  ;
  //derivata 4 di a in dx variabile d4a_dx4;
  ;
  double d4a_dx4 = (sa * sf2 * W * d3f_dx3) / (a * cf2);
  d4a_dx4 += (sa * W * d3f_dx3) / a;
  d4a_dx4 -= (e * sa * sf2 * d3f_dx3) / (a * W);
  d4a_dx4 += (6 * sa * sf3 * W * df_dx * d2f_dx2) / (a * cf3);
  d4a_dx4 += (6 * sa * sf * W * df_dx * d2f_dx2) / (a * cf);
  d4a_dx4 -= (3 * e * sa * sf3 * df_dx * d2f_dx2) / (a * cf * W);
  d4a_dx4 -= (9 * e * sa * cf * sf * df_dx * d2f_dx2) / (a * W);
  d4a_dx4 -= (3 * e * sa * cf * sf3 * df_dx * d2f_dx2) / (a * W3);
  d4a_dx4 += (3 * ca * da_dx * sf2 * W * d2f_dx2) / (a * cf2);
  d4a_dx4 += (3 * ca * da_dx * W * d2f_dx2) / a;
  d4a_dx4 -= (3 * e * ca * da_dx * sf2 * d2f_dx2) / (a * W);
  d4a_dx4 += (6 * sa * sf4 * W * pow(df_dx, 3)) / (a * cf4);
  d4a_dx4 += (8 * sa * sf2 * W * pow(df_dx, 3)) / (a * cf2);
  d4a_dx4 += (2 * sa * W * pow(df_dx, 3)) / a;
  d4a_dx4 -= (3 * e * sa * sf4 * pow(df_dx, 3)) / (a * cf2 * W);
  d4a_dx4 -= (2 * e * sa * sf2 * pow(df_dx, 3)) / (a * W);
  d4a_dx4 -= (3 * e * sa * cf2 * pow(df_dx, 3)) / (a * W);
  d4a_dx4 -= (6 * e * sa * cf2 * sf2 * W3 * pow(df_dx, 3)) / a;
  d4a_dx4 -= (3 * e3 * sa * cf2 * sf4 * W5 * pow(df_dx, 3)) / a;
  d4a_dx4 += (6 * ca * da_dx * sf3 * W * pow(df_dx, 2)) / (a * cf3);
  d4a_dx4 += (6 * ca * da_dx * sf * W * pow(df_dx, 2)) / (a * cf);
  d4a_dx4 -= (3 * e * ca * da_dx * sf3 * pow(df_dx, 2)) / (a * cf * W);
  d4a_dx4 -= (9 * e * ca * da_dx * cf * sf * pow(df_dx, 2)) / (a * W);
  d4a_dx4 -= (3 * e * ca * da_dx * cf * sf3 * W3 * pow(df_dx, 2)) / a;
  d4a_dx4 += (3 * ca * d2a_dx2 * sf2 * W * df_dx) / (a * cf2);
  d4a_dx4 -= (3 * sa * pow(da_dx, 2) * sf2 * W * df_dx) / (a * cf2);
  d4a_dx4 += (3 * ca * d2a_dx2 * W * df_dx) / a;
  d4a_dx4 -= (3 * sa * pow(da_dx, 2) * W * df_dx) / a;
  d4a_dx4 -= (3 * e * ca * d2a_dx2 * sf2 * df_dx) / (a * W);
  d4a_dx4 += (3 * e * sa * pow(da_dx, 2) * sf2 * df_dx) / (a * W);
  d4a_dx4 += (ca * d3a_dx3 * sf * W) / (a * cf);
  d4a_dx4 -= (3 * sa * da_dx * d2a_dx2 * sf * W) / (a * cf);
  d4a_dx4 -= (ca * pow(da_dx, 3) * sf * W) / (a * cf);
  ;
  ;
  //derivata 5 di f in dx  variabile d5f_dx5;
  ;
  double d5f_dx5 = (-(3 * e * ca * cf * sf * W * d4f_dx4) / (e12 * a));
  d5f_dx5 += (12 * e * ca * sf2 * W * df_dx * d3f_dx3) / (e12 * a);
  d5f_dx5 -= (12 * e * ca * cf2 * W * df_dx * d3f_dx3) / (e12 * a);
  d5f_dx5 += (12 * e * ca * cf2 * sf2 * df_dx * d3f_dx3) / (e12 * a * W);
  d5f_dx5 += (12 * e * sa * da_dx * cf * sf * W * d3f_dx3) / (e12 * a);
  d5f_dx5 += (9 * e * ca * sf2 * W * pow(d2f_dx2, 2)) / (e12 * a);
  d5f_dx5 -= (9 * e * ca * cf2 * W * pow(d2f_dx2, 2)) / (e12 * a);
  d5f_dx5 += (9 * e * ca * cf2 * sf2 * pow(d2f_dx2, 2)) / (e12 * a * W);
  d5f_dx5 += (72 * e * ca * cf * sf * W * pow(df_dx, 2) * d2f_dx2) / (e12 * a);
  d5f_dx5 -= (54 * e * ca * cf * sf3 * pow(df_dx, 2) * d2f_dx2) / (e12 * a * W);
  d5f_dx5 += (54 * e * ca * cf3 * sf * pow(df_dx, 2) * d2f_dx2) / (e12 * a * W);
  d5f_dx5 += (18 * e3 * ca * cf3 * sf3 * W3 * pow(df_dx, 2) * d2f_dx2) / (e12 * a);
  d5f_dx5 -= (36 * e * sa * da_dx * sf2 * W * df_dx * d2f_dx2) / (e12 * a);
  d5f_dx5 += (36 * e * sa * da_dx * cf2 * W * df_dx * d2f_dx2) / (e12 * a);
  d5f_dx5 -= (36 * e * sa * da_dx * cf2 * sf2 * df_dx * d2f_dx2) / (e12 * a * W);
  d5f_dx5 += (18 * e * sa * d2a_dx2 * cf * sf * W * d2f_dx2) / (e12 * a);
  d5f_dx5 += (18 * e * ca * pow(da_dx, 2) * cf * sf * W * d2f_dx2) / (e12 * a);
  d5f_dx5 -= (12 * e * ca * sf2 * W * pow(df_dx, 4)) / (e12 * a);
  d5f_dx5 += (12 * e * ca * cf2 * W * pow(df_dx, 4)) / (e12 * a);
  d5f_dx5 += (9 * e * ca * sf4 * pow(df_dx, 4)) / (e12 * a * W);
  d5f_dx5 -= (66 * e * ca * cf2 * sf2 * pow(df_dx, 4)) / (e12 * a * W);
  d5f_dx5 += (9 * e * ca * cf4 * pow(df_dx, 4)) / (e12 * a * W);
  d5f_dx5 -= (18 * e3 * ca * cf2 * sf4 * W3 * pow(df_dx, 4)) / (e12 * a);
  d5f_dx5 += (18 * e3 * ca * cf4 * sf2 * W3 * pow(df_dx, 4)) / (e12 * a);
  d5f_dx5 += (9 * e4 * ca * cf4 * sf4 * W5) * pow(df_dx, 4) / (e12 * a);
  d5f_dx5 -= (48 * e * sa * da_dx * cf * sf * W * pow(df_dx, 3)) / (e12 * a);
  d5f_dx5 += (36 * e * sa * da_dx * cf * sf3 * pow(df_dx, 3)) / (e12 * a * W);
  d5f_dx5 -= (36 * e * sa * da_dx * cf3 * sf * pow(df_dx, 3)) / (e12 * a * W);
  d5f_dx5 -= (12 * e3 * sa * da_dx * cf3 * sf3 * W3 * pow(df_dx, 3)) / (e12 * a);
  d5f_dx5 -= (18 * e * sa * d2a_dx2 * sf2 * W) * pow(df_dx, 2) / (e12 * a);
  d5f_dx5 -= (18 * e * ca * pow(da_dx, 2) * sf2 * W) * pow(df_dx, 2) / (e12 * a);
  d5f_dx5 += (18 * e * sa * d2a_dx2 * cf2 * W) * pow(df_dx, 2) / (e12 * a);
  d5f_dx5 += (18 * e * ca * pow(da_dx, 2) * cf2 * W * pow(df_dx, 2)) / (e12 * a);
  d5f_dx5 -= (18 * e * sa * d2a_dx2 * cf2 * sf2 * pow(df_dx, 2)) / (e12 * a * W);
  d5f_dx5 -= (18 * e * ca * pow(da_dx, 2) * cf2 * sf2 * pow(df_dx, 2)) / (e12 * a * W);
  d5f_dx5 += (12 * e * sa * d3a_dx3 * cf * sf * W * df_dx) / (e12 * a);
  d5f_dx5 += (36 * e * ca * da_dx * d2a_dx2 * cf * sf * W * df_dx) / (e12 * a);
  d5f_dx5 -= (12 * e * sa * pow(da_dx, 3) * cf * sf * W * df_dx) / (e12 * a);
  d5f_dx5 -= (sa * d4a_dx4 * W3) / (e12 * a);
  d5f_dx5 -= (4 * ca * da_dx * d3a_dx3 * W3) / (e12 * a);
  d5f_dx5 -= (3 * ca * pow(d2a_dx2, 2) * W3) / (e12 * a);
  d5f_dx5 += (6 * sa * pow(da_dx, 2) * d2a_dx2 * W3) / (e12 * a);
  d5f_dx5 += (ca * pow(da_dx, 4) * W3) / (e12 * a);

//  double tt1 = fi + df_dx * s;
// double tt2 = d2f_dx2 * pow(s, 2) / 2;
// double tt3 = d3f_dx3 * pow(s, 3) / 6;
// double tt4 = d4f_dx4 * pow(s, 4) / 24;
// double tt5 = d5f_dx5 * pow(s, 5) / 120;


  return fff;
}

double Geodesia::las(double s, double alfa, int un)
{
  alfa = ang_conv(alfa, un);

  if (abs(sin(alfa)) < __DBL_EPSILON__) //cammina lungo il meridiano
    return _lambda;

  double t1 = _lambda + s * sin(alfa) / (GN * cos(fi));
  double t2 = pow(s, 2) * sin(fi) * sin(2 * alfa) / (2 * pow(GN, 2) * pow(cos(fi), 2));
  double t3 = pow(s, 3) / (6 * pow(GN, 2) * cos(fi));
  t3 *= (sin(2 * alfa) * cos(alfa) / ro + 2 * pow(tan(fi), 2) * sin(3 * alfa) / GN);
  double fff = t1 + t2 + t3;

  double p = s * cos(alfa) / ro;
  double q = s * sin(alfa) / r;
 // double sin2f = sin(2e0 * fi);
 // double sin2f2 = sin2f * sin2f;
  double sinf = sin(fi);
  double sinf2 = sinf * sinf;
//  double GN2 = GN * GN;
//  double a = semiasse;
//  double a2 = a * a;
  double ffc = _lambda + q;
  ffc += (ro * sinf / r) * p * q;
  ffc += (ro / r) * ((ro / r) * sinf2 + cos(fi) / 3) * pow(p, 2) * q;
  ffc -= (sinf2 / 3) * pow(q, 3);
#ifdef _DEBUG
  double diff = (fff - ffc) / ARCS;
  cout << diff;
#endif
  return ffc;



}

void Geodesia::fi_la_step(double s, double alfa, double& la, double& lo, int un)
// trasporto geografiche diretto
{
  alfa = ang_conv(alfa, un);
  double fi0 = fi;
  double la0 = la;


  if (s > 0)
  {
    double ss_i = s / 4.0e0;
    double ss = 0;
    for (int i = 0; i < 4; i++)
    {
      double alfa_a = alfa;
      lo = las(ss_i, alfa, i_u_r);
      la = fis(ss_i, alfa, i_u_r);
      calcola(i_u_r, la, lo);
      double  fim = (la + fi) / 2;
      alfa = alfa_a + (lo - _lambda) * sin(fim);
      ss += ss_i;
      if (ss > s) break;
    }
 //   double sss = ss - s;
  }
  else
  {
    alfa += PI;
    if (alfa > 2 * PI) alfa -= 2 * PI;
    double ss_i = s / 4.0e0;
    double ss = 0;
    for (int i = 0; i < 4; i++)
    {
 //     double lo_a = lo;
//      double fi_a = fi;
      double alfa_a = alfa;
      lo = las(-ss_i, alfa, i_u_r);
      la = fis(-ss_i, alfa, i_u_r);
      calcola(i_u_r, la, lo);
      double  fim = (la + fi) / 2;
      alfa = alfa_a + (lo - _lambda) * sin(fim);
      ss += ss_i;
      if (abs(ss) > abs(s)) break;
    }
 //   double sss = ss - s;
  }
  calcola(i_u_r, fi0, la0);
}


void Geodesia::fi_la(double s, double alfa, double& la, double& lo, int un)
// trasporto geografiche diretto
{
  // return fi_la_step(s, alfa, la, lo, un);
  alfa = ang_conv(alfa, un);
  if (s > 0)
  {
    lo = las(s, alfa, i_u_r);
    la = fis(s, alfa, i_u_r);
  }
  else
  {
    alfa += PI;
    if (alfa > 2 * PI) alfa -= 2 * PI;
    lo = las(-s, alfa, i_u_r);
    la = fis(-s, alfa, i_u_r);
  }
}

void Geodesia::s_alfa(double fi0, double la0, double fi1, double la1, double& s, double& az1, double& az2, int un)//radianti
//trasporto cordinate inverso
{
  fi0 = ang_conv(fi0, un);
  la0 = ang_conv(la0, un);
  fi1 = ang_conv(fi1, un);
  la1 = ang_conv(la1, un);
  double bck_fi = fi;
  double bck_la = _lambda;
  calcola(i_u_r, fi0, la0);

  double N0 = GN;
  double N02 = N0 * N0;
  double Ro0 = ro;
  double R0 = r;
  double a2 = semiasse * semiasse;
  double sinf = sin(fi0);
  double sin2f = sin(2e0 * fi0);
  double d_f = fi1 - fi0;
  double d_l = la1 - la0;


  double p_p = d_f;
  p_p += (3e0 * ecc * N02 * sin2f) / (4e0 * a2) * d_f * d_f;
  p_p += N0 * sin2f / (4e0 * Ro0) * d_l * d_l;
  p_p += (ecc * N02 / (2e0 * a2)) * ((5e0 * ecc * N02 * pow(sin2f, 2e0)) / (4e0 * a2) + cos(2e0 * fi0)) * pow(d_f, 3e0);
  p_p += (R0 * cos(fi0) / (6e0 * Ro0) - pow(sinf, 2e0) / 2e0) * d_f * d_l * d_l;
  double q_q = d_l;
  q_q -= Ro0 * sinf * d_l * d_f / R0;
  q_q -= Ro0 * ((3e0 * ecc * N02 * pow(sinf, 2e0)) / a2 + 2.0e0 / 3.0e0) / (2e0 * N0) * d_f * d_f * d_l;
  q_q -= pow(sinf, 2e0) * pow(d_l, 3e0) / 6.0e0;
  double azimutgeodetica = atan2((q_q * R0), (p_p * Ro0));

  if (azimutgeodetica < 0) azimutgeodetica += 2e0 * PI;
  az1 = azimutgeodetica;
  double arcogeodeticab = p_p * Ro0 / cos(azimutgeodetica);
  double arcogeodeticac = q_q * R0 / sin(azimutgeodetica);
  double arcogeodetica = (arcogeodeticab + arcogeodeticac) / 2;
  s = arcogeodetica;
  double fim = (fi0 + fi1) / 2;
  az2 = az1 + (la1 - la0) * sin(fim);
  calcola(i_u_r, bck_fi, bck_la);
}

void Geodesia::DaCSaGB(double fi_orig, double la_orig, double n, double e,
  double& arco_bessel, double& az1_bessel, double& az2_bessel,
  double& arco_hayford, double& az1_hayford, double& az2_hayford,
  double& lat_bessel, double& lon_bessel,
  double& lat_hayford, double& lon_hayford, int un)
{
  const double latMm = 41.552551e0, lonMm = 12.270840e0;
  fi_orig = ang_conv(fi_orig, un);
  la_orig = ang_conv(la_orig, un);
  //settiamo ellissoide di bessel
  SetAsse(6377397.155000);
  SetEcc(6.674372230000e-3);
  //double fi_bck = daras(fi); double la_bck = daras(_lambda);
  calcola(i_u_r, (fi_orig), (la_orig)); //setta l'origine CS

  double lat, lon, s, az1, az2;

  //calcolo diretto
  fi_la(n, 0e0, lat, lon, i_u_r);
  calcola(i_u_r, lat, lon); //setta nuova origine origine CS
  fi_la(e, PI / 2, lat, lon, i_u_r);
  s_alfa(fi_orig, la_orig, lat, lon, s, az1, az2, i_u_r);//calcolo inverso
  arco_bessel = s;
  az1_bessel = az1;
  az2_bessel = az2;
  lat_bessel = lat;
  lon_bessel = lon;
  az1 += (0.88e0 - 4.96e0) * ARCS; //azmut hayford Mmario monte soratte 6,350088
                                          //meno azimut Bessel Mmario monte soratte 6,350496
                                          //= 0.88-4.96 secondi
  az2 += (0.88e0 - 4.96e0) * ARCS;
  SetAsse(); //hayford
  SetEcc();
  calcola();
  fi_la(s, az1, lat, lon, i_u_r); //diretto su hayfor
  lat_hayford = lat;
  lon_hayford = lon;
  s_alfa(dasar(latMm), dasar(lonMm), lat, lon, s, az1, az2, i_u_r);//calcolo inverso su hayford
  arco_hayford = s;
  az1_hayford = az1;
  az2_hayford = az2;

}

void Geodesia::FiLagb(double N, double E, double& fifi, double& lala)
{
  double actualfi = dagas(fi * RGC);
  double actualla = dagas(_lambda * RGC);
  double actualasse = semiasse;
  double actualecc = ecc;
  SetAsse();  /*setta semiasse hayford                */
  SetEcc();   /*setta eccentricita'hayford            */
  calcola();  /*setta fi,lamda di M.mario             */
  double meridOvest = dasar(9.0e0);
  double meridEst = dasar(15.0e0);
  double DELTAE = 0;
  if (E < 2010000)
  {
    DELTAE = E - 1500000;
    lala = meridOvest;
  }
  else
  {
    DELTAE = E - 2520000;
    lala = meridEst;
  }
  double ecc4 = ecc * ecc;
  double ecc6 = ecc4 * ecc;
  double ecc8 = ecc6 * ecc;
  double m = (1 + ecc / 4 + ecc4 * 7 / 64 + ecc6 * 15 / 256 + ecc8 * 579 / 16384) / (crid * semiasse);
  double XXX = m * N;
  double YYY = m * DELTAE;
  double l0 = ecc / 4 + 3 * ecc4 / 64 + 5 * ecc6 / 256 + 175 * ecc8 / 16384;
  double l1 = ecc4 / 24 + 35 * ecc6 / 768 + 71 * ecc8 / 1536;
  double l2 = 127 * ecc6 / 5808 + 9803 * ecc8 / 185856;
  double l3 = 33239 * ecc8 / 1626240;

  double AAA = sin(XXX) * cosh(YYY);
  double AAA2 = AAA * AAA;
  double AAA3 = AAA2 * AAA;
  double AAA4 = AAA3 * AAA;
  double AAA5 = AAA4 * AAA;
  double AAA6 = AAA5 * AAA;
  double AAA7 = AAA6 * AAA;

  double BBB = cos(XXX) * sinh(YYY);
  double BBB2 = BBB * BBB;
  double BBB3 = BBB2 * BBB;
  double BBB4 = BBB3 * BBB;
  double BBB5 = BBB4 * BBB;
  double BBB6 = BBB5 * BBB;
  double BBB7 = BBB6 * BBB;


  double alf = asin(sin(XXX) / cosh(YYY));
  double ni = log(tan(PI / 4 + alf / 2)) - l0 * AAA + l1 * (AAA3 - 3 * AAA * BBB2) -
    l2 * (AAA5 - 10 * AAA3 * BBB2 + 5 * AAA * BBB4) +
    l3 * (AAA7 - 21 * AAA5 * BBB2 + 35 * AAA3 * BBB4 - 7 * AAA * BBB6);

  double psi = 2 * (atan(exp(ni)) - PI / 4);
  fifi = psi + sin(2 * psi) * (ecc / 2) * ((1 + ecc + ecc4 + ecc6) -
    (7 * ecc + 17 * ecc4 + 30 * ecc6) * pow(sin(psi), 2e0) / 6e0 +
    (224 * ecc4 + 889 * ecc6) * pow(sin(psi), 4e0) / 120e0 -
    (ecc6 * pow(sin(psi), 6e0)) * 4279 / 1260);
  double beta = atan(sinh(YYY) / cos(XXX));
  lala += (beta - l0 * BBB + l1 * (3 * AAA2 * BBB - BBB3) - l2 * (5 * AAA4 * BBB - 10 * AAA2 * BBB3 + BBB5) +
    l3 * (7 * AAA6 * BBB - 35 * AAA4 * BBB3 + 21 * AAA2 * BBB5 - BBB7));

  SetAsse(actualasse);  /*setta semiasse hayford                */
  SetEcc(actualecc);   /*setta eccentricita'hayford            */
  calcola(i_u_s, actualfi, actualla);  /*resetta fi,lamda              */

}
double Geodesia::sec(double x) { return 1e0 / cos(x); }
double Geodesia::cosec(double x) { return 1e0 / sin(x); }
double Geodesia::lat_geoc() { return  atan(z / r); }
double Geodesia::lat_ridotta(double lat)
{
 // double actualfi = fi;
 // double actualla = _lambda;
  calcola(i_u_r, lat, 0e0);

  double a = semiasse;
  double b = a * sqrt(1e0 - ecc);
  
  double oe = 2e0 * atan(sqrt((a - b) / (a + b)));
#ifdef _DEBUG
  double f = (a - b) / a;
  double vf = 1e0 - cos(oe) - f;
  cout << vf;
#endif
  double latg = lat_geoc();
  double u = sec(oe) * tan(latg);
  u = atan(u);
  calcola(i_u_r, fi, _lambda);
  return u;

}



dacsagb::dacsagb(double par1, double par2, int par3)
{
  op = par3;
  if ((op & dapianeapiane) || (op & dapianeageogr))
  {
    EstCS = par1;
    NordCS=par2;
  }
  if ((op & dageograpiane) || (op & dageograagegr))
  {
    fiCS = par1;
    laCS = par2;
  }
  if (dapianeageogr)
  {
    double par3= VALORE_NUMERICO_NULLO, par4= VALORE_NUMERICO_NULLO,
      par5= VALORE_NUMERICO_NULLO, par6= VALORE_NUMERICO_NULLO, par7= VALORE_NUMERICO_NULLO, par8 = VALORE_NUMERICO_NULLO;
    double par9 = VALORE_NUMERICO_NULLO, par10 = VALORE_NUMERICO_NULLO;
    calcolo(par1, par2, par3, par4, par5, par6,par7,par8,par9,par10);

  }


}

bool dacsagb::calcolaCXF(char* filemp, char* fileout, char* filelog, char* cartellagbs, int fmt)
{
  return calcolaCXFC(filemp, fileout, filelog, cartellagbs,fmt);
}

bool dacsagb::calcolaCXFC(std::string filemp, std::string fileout, std::string filelog, std::string cartellagbs,int fmt)
{


  string nomemappa;
  GRI_HDR* h;
  FILE *impfilecxf;


  switch(fmt)
{ 
  case(o_u_s):
      ses = true;
  break;
  case(o_u_r):  
    rad = true;
  break;

  case(o_u_c):
    cent = true;
  break;

  case(o_u_d):
    deg = true;
  break;

  case(o_u_p):
    piane = true;
  break;
  default:
    piane = true;

}
  struct stat buff_stat;
nomefilecxf = filemp; nomefileout = fileout; nomefilelog = filelog; cartellageo = cartellagbs;

  if (nomefilelog.at(0) == '\0') nomefilelog = "default.log";
  
  if(-1!=stat(nomefilelog.c_str(), &buff_stat))
  {

      perror("stat");
      remove(nomefilelog.c_str());
  }
  /*
  else
  {
    
    printf("File type:                ");

    switch (buff_stat.st_mode & S_IFMT) {
    case S_IFBLK:  printf("block device\n");            break;
    case S_IFCHR:  printf("character device\n");        break;
    case S_IFDIR:  printf("directory\n");               break;
    case S_IFIFO:  printf("FIFO/pipe\n");               break;
    case S_IFLNK:  printf("symlink\n");                 break;
    case S_IFREG:  printf("regular file\n");            break;
    case S_IFSOCK: printf("socket\n");                  break;
    default:       printf("unknown?\n");                break;
    }

    printf("I-node number:            %ld\n", (long)buff_stat.st_ino);

    printf("Mode:                     %lo (octal)\n",
      (unsigned long)buff_stat.st_mode);

    printf("Link count:               %ld\n", (long)sb.st_nlink);
    printf("Ownership:                UID=%ld   GID=%ld\n",
      (long)sb.st_uid, (long)buff_stat.st_gid);

    printf("Preferred I/O block size: %ld bytes\n",
      (long)buff_stat.st_blksize);
    printf("File size:                %lld bytes\n",
      (long long)buff_stat.st_size);
    printf("Blocks allocated:         %lld\n",
      (long long)buff_stat.st_blocks);

    printf("Last status change:       %s", ctime(&buff_stat.st_ctime));
    printf("Last file access:         %s", ctime(&buff_stat.st_atime));
    printf("Last file modification:   %s", ctime(&buff_stat.st_mtime));


  }
*/
  error_t err;
  logfile.open(nomefilelog, std::ios::out);
  if (logfile.fail())
  {
    cout << "fallita apertura file" << nomefilelog;
    int result = stat(nomefilelog.c_str(), &buff_stat);

    // Check if statistics are valid:
    if (result != 0)
    {
      perror("Problem getting information");
      switch (errno)
      {
      case ENOENT:
        printf("File %s not found.\n", nomefilelog);
        break;
      case EINVAL:
        printf("Invalid parameter to stat.\n");
        break;
      default:
        /* Should never be reached. */
        printf("Unexpected error in stat.\n");
      }
    }


    return false;
  }
  if (-1 == stat(nomefilecxf.c_str(), &buff_stat))
  {
    if((buff_stat.st_mode& S_IFMT)== S_IFDIR)
    {
      logfile << "la cartella con i cxf " << nomefilecxf << " non esiste" << endl;
    }
    else
    {
      logfile << "il file cxf " << nomefilecxf << " non esiste" << endl;
    }
    logfile << "stop" << endl;
    return -1; 
  }
  if (-1 != stat(nomefileout.c_str(), &buff_stat))
  {
    if ((buff_stat.st_mode & S_IFMT) == S_IFDIR)
    {
      string elenco = nomefileout;
      elenco += "/*.*";
      remove(elenco.c_str());
      logfile << "svuotata cartella " << nomefileout << endl;
    }
    else
    {
      remove(nomefileout.c_str());
      logfile << "rimosso il file dei risultati precedente" << nomefileout << endl;
    }
  }
  else
  {
    if (errno == ENOENT)
    {
      if (nomefileout.find(".ctf") == string::npos)
      {
        logfile << "la cartella per i risultati " << nomefileout << " non esiste" << endl;
        logfile << "stop" << endl;
        return -1;
      }
    }
  }
   
  if (-1 == stat(cartellageo.c_str(), &buff_stat))
  {
   
    logfile << "la cartella con i grigliati " << cartellageo << " non esiste" << endl;
    logfile << "stop" << endl;
    return false;
   }
  cout << "file log " << nomefilelog << endl;
  if ((buff_stat.st_mode & S_IFMT) == S_IFDIR)
  {
    logfile << "Cartella con i gligliati GeoServer " << cartellageo.c_str() << endl;
  }
  else
    logfile << "File con i gligliati GeoServer " << cartellageo.c_str() << endl;
  if (-1 != stat(nomefilecxf.c_str(), &buff_stat))
  {
    if ((buff_stat.st_mode & S_IFMT) == S_IFDIR)
      logfile << "cartella con file CXF " << nomefilecxf << endl;
    else
      logfile << "file CXF " << nomefilecxf << endl;
  }
  else
  {
    if (errno == ENOENT)
    {
      if (nomefilecxf.find(".cxf") != string::npos)
      {
        logfile << "la cartella per i file cxf " << nomefilecxf << " non esiste" << endl;
        logfile << "stop" << endl;
        return -1;
      }
      else
      {
        logfile << "il file cxf " << nomefilecxf << " non esiste" << endl;
        logfile << "stop" << endl;
        return -1;
      }
    }
  }
 
  logfile << " File con le coordinate trasformate" << nomefileout.c_str() << endl;
  logfile << "formato output " << "ses = " << ses << " deg= " << deg << " piane= " << piane << endl;

  
  string ext = ".gsb";
  string tipof = "R40_F00";
  file_gsb.clear();
  stat(cartellageo.c_str(), &buff_stat);
  if ((buff_stat.st_mode & S_IFMT) == S_IFDIR)
  {

    struct dirent** namelist;
    int n;
    n = scandir(cartellageo.c_str(), &namelist, 0, alphasort);
    if (n < 0)
    {
    }
   
    for (int k=0;k<n;k++)
    {
      string nmfile = namelist[k]->d_name;
      cout << nmfile << endl;

      if (nmfile.find(ext)!=string::npos)
      {
        
        if (nmfile.find("R40_F00") != string::npos) //
        {
          file_gsb = nmfile;//trovato
          nomefilegriglia = cartellageo + "/" + file_gsb;
          break;
        }
      }
    }
  }
  else
  {
    file_gsb = cartellageo;
    nomefilegriglia = file_gsb;
  }
  int type=GRI_FILE_TYPE_BIN;
  int *prc=NULL;
  h = gri_load_file(nomefilegriglia.c_str(),false,true,NULL ,prc);
 
  char buffer[100];
  //recupero il nome della mmappa
  string s = nomefilecxf;
  auto pos = s.find_last_of('/');
  string s1 = s.substr(pos + 1);
  pos = s1.find_last_of('.');
  nomemappa = s1.substr(0, pos + 1);

  outfile.open(nomefileout, std::ios::out);
  if (outfile.fail())
  {
    cout << "fallita apertura file" << nomefileout;
    return -1;
  }
  impfilecxf = fopen(nomefilecxf.c_str(), "r");
  if (impfilecxf==NULL)
  {
    cout << "fallita apertura file" << nomefilecxf;
    return -1;
  }

  vector<string> linp;
  /*riga per riga*/
  while (!feof(impfilecxf))
  {
    fgets(buffer, 100, impfilecxf);
     cout << buffer << endl;
    string sb = buffer;
    linp.push_back(buffer);
  }
  fclose(impfilecxf);
  double estUTM, nordUTM;
  int fuso;
  for (auto p = linp.begin(); p < linp.end(); p++)
  {
    /* vediamo se � un valore coordinate*/
    if ((*p).find("MAPPA") != string::npos || (*p).find("QUADRO") != string::npos)
    {
      outfile << (*p).c_str() << endl; //etichetta mappa
      p++;
      outfile << (*p).c_str() << endl;//nome mappa
      p++;
      outfile << (*p).c_str() << endl;//scala
    }
    if ((*p).find("BORDO") != string::npos)
    {
      outfile << (*p).c_str() << endl; //Bordo
      p++;
      string etichetta = *p;
      outfile << (*p).c_str() << endl;//dentificativo bordo
      p++;
      outfile << (*p).c_str() << endl;//scala
      p++;
      outfile << (*p).c_str() << endl;//angolo
      p++;
      double  POSIZIONEX = atof((*p).c_str());
      p++;
      double  POSIZIONEY = atof((*p).c_str());
      calcolaunpunto(h, POSIZIONEX, POSIZIONEY, nordUTM, estUTM, fuso);
      outfile.width(12);outfile.precision(3);
      outfile << fixed<< right<<estUTM << endl;
      outfile.width(12); outfile.precision(3);
      outfile << fixed<< right<<nordUTM << endl;
      p++;
      double  POSIZIONEPUNTOINTERNOX = atof((*p).c_str());
      p++;
      double  POSIZIONEPUNTOINTERNOY = atof((*p).c_str());
      calcolaunpunto(h, POSIZIONEPUNTOINTERNOX, POSIZIONEPUNTOINTERNOX, nordUTM, estUTM, fuso);
      outfile.width(12); outfile.precision(3);
      outfile << fixed << right << estUTM << endl;
      outfile.width(12); outfile.precision(3);
      outfile << fixed << right << nordUTM << endl;
      p++;
      outfile << (*p).c_str() << endl;//numero isole
      int num_isole = atoi((*p).c_str());
      p++;
      outfile << (*p).c_str() << endl;//numero vertici
      int num_vertici = atoi((*p).c_str());
      if (num_isole > 0)
      {
        for (int k = 0; k < num_isole; k++)
        {
          p++;
          int num_vertici_isole = atoi((*p).c_str());
          outfile << (*p).c_str() << endl;//numero vertici k-ma isola
        }
      }
      for (int i = 0; i < num_vertici; i++)
      {
        p++;
        POSIZIONEX = atof((*p).c_str());
        p++;
        POSIZIONEY = atof((*p).c_str());
        calcolaunpunto(h, POSIZIONEX, POSIZIONEY, nordUTM, estUTM, fuso);
        outfile.width(12); outfile.precision(3);
        outfile << fixed << right << estUTM << endl;
        outfile.width(12); outfile.precision(3);
        outfile << fixed << right << nordUTM << endl;
        
      }
    }
    if ((*p).find("TESTO") != string::npos)
    {
      outfile << (*p).c_str() << endl; //etichetta testo
      p++;
      outfile << (*p).c_str() << endl;//contenuto
      p++;
      outfile << (*p).c_str() << endl;//dimensione
      p++;
      outfile << (*p).c_str() << endl;//angolo
      p++;
      double estGB, nordGB;
      double  POSIZIONEX = atof((*p).c_str());
      p++;
      double  POSIZIONEY = atof((*p).c_str());
      bool rt = calcolaunpunto(h,POSIZIONEX, POSIZIONEY, nordUTM, estUTM, fuso);
      outfile.width(12); outfile.precision(3);
      outfile << fixed << right << estUTM << endl;
      outfile.width(12); outfile.precision(3);
      outfile << fixed << right << nordUTM << endl;
    }
    if ((*p).find("SIMBOLO") != string::npos)
    {
      outfile << (*p).c_str() << endl; //etichettaSIMBOLO
      p++;
      outfile << (*p).c_str() << endl;//codice del simbolo
      p++;
      outfile << (*p).c_str() << endl;//angolo
      p++;
      double estGB, nordGB;
      double  POSIZIONEX = atof((*p).c_str());
      p++;
      double  POSIZIONEY = atof((*p).c_str());
      bool rt = calcolaunpunto(h,POSIZIONEX, POSIZIONEY, nordUTM, estUTM, fuso);
      outfile.width(12); outfile.precision(3);
      outfile << fixed << right << estUTM << endl;
      outfile.width(12); outfile.precision(3);
      outfile << fixed << right << nordUTM << endl;
    }
    if ((*p).find("FIDUCIALE") != string::npos)
    {
      outfile << (*p).c_str() << endl; //etichetta FIDUCIALE
      p++;
      outfile << (*p).c_str() << endl;//codice del fiduciale
      p++;
      outfile << (*p).c_str() << endl;//angolo
      p++;
      double estGB, nordGB;
      double  POSIZIONEX = atof((*p).c_str());
      p++;
      double  POSIZIONEY = atof((*p).c_str());
      int rt = calcolaunpunto(h,POSIZIONEX, POSIZIONEY, nordUTM, estUTM, fuso);
      outfile.width(12); outfile.precision(3);
      outfile << fixed << right << estUTM << endl;
      outfile.width(12); outfile.precision(3);
      outfile << fixed << right << nordUTM << endl;
      p++;
      double  POSIZIONEPUNTOINTERNOX = atof((*p).c_str());
      p++;
      double  POSIZIONEPUNTOINTERNOY = atof((*p).c_str());
      rt = calcolaunpunto(h,POSIZIONEPUNTOINTERNOX, POSIZIONEPUNTOINTERNOY, nordUTM, estUTM, fuso);
      outfile.width(12); outfile.precision(3);
      outfile << fixed << right << estUTM << endl;
      outfile.width(12); outfile.precision(3);
      outfile << fixed << right << nordUTM << endl;
    }
    if ((*p).find("LINEA") != string::npos)
    {
      outfile << (*p).c_str() << endl; //LINEA
      p++;
      outfile << (*p).c_str() << endl;//tipo tratto
      p++;
      outfile << (*p).c_str() << endl;//numero vertici
      int num_vertici = atoi((*p).c_str());
      for (int i = 0; i < num_vertici; i++)
      {
        p++;
        double POSIZIONEX = atof((*p).c_str());
        p++;
        double POSIZIONEY = atof((*p).c_str());
        int rt = calcolaunpunto(h,POSIZIONEX, POSIZIONEY, nordUTM, estUTM, fuso);
        outfile.width(12); outfile.precision(3);
        outfile << fixed << right << estUTM << endl;
        outfile.width(12); outfile.precision(3);
        outfile << fixed << right << nordUTM << endl;

      }
    }
    if ((*p).find("RIFERIMENTO_RASTER") != string::npos)
    {
      outfile << (*p).c_str() << endl; //etichetta RIFERIMENTO_RASTER
      p++;
      outfile << (*p).c_str() << endl;//nome del RIFERIMENTO_RASTER
      for (int i = 0; i < 4; i++)
      {
        p++;
        double POSIZIONEX = atof((*p).c_str());
        p++;
        double POSIZIONEY = atof((*p).c_str());
        int rt = calcolaunpunto(h,POSIZIONEX, POSIZIONEY, nordUTM, estUTM, fuso);
        outfile.width(12); outfile.precision(3);
        outfile << fixed << right << estUTM << endl;
        outfile.width(12); outfile.precision(3);
        outfile << fixed << right << nordUTM << endl;
      }
    }
    if ((*p).find("LIBRETTO") != string::npos)
    {
      outfile << (*p).c_str() << endl; //LIBRETTO
      p++;
      outfile << (*p).c_str() << endl;//potocollo
      p++;
      outfile << (*p).c_str() << endl;//tipo tratto
      p++;
      outfile << (*p).c_str() << endl;//numero vertici
      int num_vertici = atoi((*p).c_str());
      for (int i = 0; i < num_vertici; i++)
      {
        p++;
        double POSIZIONEX = atof((*p).c_str());
        p++;
        double POSIZIONEY = atof((*p).c_str());
        int rt = calcolaunpunto(h,POSIZIONEX, POSIZIONEY, nordUTM, estUTM, fuso);
        outfile.width(12); outfile.precision(3);
        outfile << fixed << right << estUTM << endl;
        outfile.width(12); outfile.precision(3);
        outfile << fixed << right << nordUTM << endl;
      }
    }
  }
  ;
 
  outfile << "EOF" << endl;
  outfile.close();
  logfile.close();
  gri_delete(h);
  return true;
}
bool DaPianeCSaFiLa(double est, double nord, double& la, double& fi, string & ms)
{
  /* */
  dacsagb c(est, nord, daCSafila | ddd_mm_ss);
  fi = c.fiCS;
  la = c.laCS;
  ms = c.messaggio;
  return true;
}
int  calcolaunpunto(double estCS, double nordCS, int fuso_richiesto, double& estGB, double& nordGB)
{
  Geodesia gi;
  double n = nordCS;
  double e = estCS;
  //settiamo ellissoide di bessel
  gi.SetAsse(6377397.155000);
  gi.SetEcc(6.674372230000e-3);
  const double latMm = 41e0 + 55e0 / 60.0e0 + 25.51e0 / 60e0 / 60e0, lonMm = 12e0 + 27e0 / 60.0e0 + 8.40e0 / 60e0 / 60e0;
  const double latMm_b_su_genova = 41e0 + 55e0 / 60.0e0 + 24.399e0 / 60e0 / 60e0, lonMm_b_su_genova = 3e0 + 31e0 / 60.0e0 + 51.131 / 60e0 / 60e0;
  gi.calcola(i_u_d, latMm_b_su_genova, lonMm_b_su_genova);
  double N0 = gi.GN;
  double N02 = N0 * N0;
  double Ro0 = gi.ro;
  double R0 = gi.r;
  double la_0 = gi._lambda;
  double fi_0 = gi.fi;
  double semiasse = gi.semiasse;
  double semiasse2 = semiasse * semiasse;
  double ecc = gi.ecc;
  double lat_mia, lon_mia, az1_mia, az2_mia;
  double lat_mia0 ;
  double lon_mia0 ;
  gi.calcola(i_u_d, latMm_b_su_genova, lonMm_b_su_genova);
  //trasporto geografiche problema diretto 
  if (n > 0)
  {
    lat_mia = gi.fis(n, 0e0, i_u_r);
    lon_mia = gi.las(n, 0e0, i_u_r);
  }
  else
  {
    lat_mia = gi.fis(-n, PI, i_u_r);
    lon_mia = gi.las(-n, PI, i_u_r);
  }
  //settiamo ellissoide di bessel
  gi.calcola(i_u_r, lat_mia, lon_mia);
  if (e > 0)
  {
    lon_mia = gi.las(e, PI / 2, i_u_r);
    lat_mia = gi.fis(e, PI / 2, i_u_r);
  }
  else
  {
    lon_mia = gi.las(-e, 3 * PI / 2, i_u_r);
    lat_mia = gi.fis(-e, 3 * PI / 2, i_u_r);
  }


  gi.calcola(i_u_r, lat_mia, lon_mia); //entra radianti
  double d_f = gi.fi - fi_0;
  double d_l = gi._lambda - la_0;
  //d_f -=fi_0;
  //d_l -=la_0;

  double p_p = d_f;
  p_p += (3e0 * ecc * N02 * sin(2e0 * fi_0)) / (4e0 * semiasse2) * d_f * d_f;
  p_p += N0 * sin(2e0 * fi_0) / (4e0 * Ro0) * d_l * d_l;
  p_p += (ecc * N02 / (2e0 * semiasse2)) * ((5e0 * ecc * N02 * pow(sin(2e0 * fi_0), 2e0)) / (4e0 * semiasse2) + cos(2e0 * fi_0)) * pow(d_f, 3e0);
  p_p += (R0 * cos(fi_0) / (6e0 * Ro0) - pow(sin(fi_0), 2e0) / 2e0) * d_f * d_l * d_l;
  double q_q = d_l;
  q_q -= Ro0 * sin(fi_0) * d_l * d_f / R0;
  q_q -= Ro0 * ((3e0 * ecc * N02 * pow(sin(fi_0), 2e0)) / (semiasse2)+2.0e0 / 3.0e0) / (2e0 * N0) * d_f * d_f * d_l;
  q_q -= pow(sin(fi_0), 2e0) * pow(d_l, 3e0) / 6.0e0;
  double azimutgeodetica = atan2((q_q * R0), (p_p * Ro0));

  if (azimutgeodetica < 0) azimutgeodetica += 2e0 * PI;
  double arcogeodeticab = p_p * Ro0 / cos(azimutgeodetica);
  double arcogeodeticac = q_q * R0 / sin(azimutgeodetica);
  double arcogeodetica = (arcogeodeticab + arcogeodeticac) / 2;
  // double azses = daras(azimutgeodetica);
  azimutgeodetica = (azimutgeodetica * 180 / PI);
  double s12, az1, az2;
  gi.SetAsse(6377397.155000);
  gi.SetEcc(6.674372230000e-3);
  double arco_bessel; double az1_bessel; double az2_bessel;
  double arco_hayford; double az1_hayford; double az2_hayford;
  double lat_bessel; double lon_bessel;
  double lat_hayford; double lon_hayford;
  gi.DaCSaGB(latMm_b_su_genova, lonMm_b_su_genova,
    n, e, arco_bessel, az1_bessel, az2_bessel,
    arco_hayford, az1_hayford, az2_hayford,
    lat_bessel, lon_bessel,
    lat_hayford, lon_hayford, i_u_d);
  lat_bessel = daras(lat_bessel);
  lon_bessel = daras(lon_bessel);
  lat_hayford = daras(lat_hayford);
  lon_hayford = daras(lon_hayford);
  az1_bessel = daras(az1_bessel);
  az2_bessel = daras(az2_bessel);
  az1_hayford = daras(az1_hayford);
  az2_hayford = daras(az2_hayford);
  Geodesia geo;
  geo.NEgb(lat_hayford, lon_hayford, false);
  if (fuso_richiesto == FUSO_OVEST)
  {
    nordGB = geo.NordFO; estGB = geo.EstFO;
  }
  if (fuso_richiesto == FUSO_EST)
  {
    nordGB = geo.NordFE; estGB = geo.EstFE;
  }
 /*
  cout.width(10);
  cout.precision(8);
 
  cout << " " << fixed << " nord " << n << " est " << e << " lat " << lat_bessel << " lon " << lon_bessel << endl;
  cout << "                                                   lat " << lat_hayford << " lon " << lon_hayford << endl;
  geo.NEgb(lat_hayford, lon_hayford, false);
  nordGB = geo.NordFE; estGB = geo.EstFO;
  cout << " Nord Fuso Ovest " << geo.NordFO << " Est Fuso Ovest " << geo.EstFO << endl;
  cout << " Nord Fuso Est   " << geo.NordFE << " Est Fuso Est   " << geo.EstFE << endl;
  cout << "----------------------------------------------------------------------------------------------------------------------------------" << endl;
  cout << fixed << " nord " << n << " est " << e << " lat " << lat_bessel << " lon " << lon_bessel << " s0i " << arco_bessel << " az1 " << az1_bessel << " az2 " << az2_bessel << endl;
  cout << "                                                   lat " << lat_hayford << " lon " << lon_hayford << " s0h " << arco_hayford << " az1 " << az1_hayford << " az2 " << az2_hayford << endl;
  geo.NEgb((lat_hayford), (lon_hayford), false);
  cout << " Nord Fuso Ovest " << geo.NordFO << " Est Fuso Ovest " << geo.EstFO << endl;
  cout << " Nord Fuso Est   " << geo.NordFE << " Est Fuso Est   " << geo.EstFE << endl;
  cout << "----------------------------------------------------------------------------------------------------------------------------------" << endl;
  char rs[400];
  sprintf(rs, "8|%s|%11.3f|%11.3f|%11.3f|%11.3f|", "punto", geo.NordFE, geo.EstFE, geo.NordFO, geo.EstFO);
  cout << rs << endl;
*/
  return 1; //OK

}
int  la_fiCS_la_fiGB(double laCS, double fiCS, int u, double& laGB, double& fiGB) { return 0; };
int  la_fiCS_est_nordCS(double laCS, double fiCS, int u, double& estCS, double& nordCS) { return 0; };
int  la_fiGB_est_nordGB(double laGB, double fiGB, int u, double& estGB, double& nordGB) { return 0; };
int  est_nordCS_la_fiCS(double estCS, double nordCS, double& laCS, double& fiCS) { return 0; };
int  est_nordGB_la_fiGB(double estGB, double nordGB, double& laGB, double& fiGB) { return 0; };




void calcolo(
  double& estCS, double& nordCS, 
  double& loCS, double& fiCS, 
  double& estGBFE, double& nordGBFE, 
  double& estGBFO, double& nordGBFO,
  double& loGB, double& fiGB)
{
    /*
   Geodesia controllo;
  controllo.NEgb(0e0, 15e0, FALSE);
  Geodesic Bessel(6377397.15, 1.0e0 / 299.15);
  Geodesic Hayford(6378388.0e0, 1.0e0 / 297.0);
  const double latMm = 41 + 55 / 60.0e0 + 25.51 / 60e0 / 60e0, lonMm = 12 + 27e0 / 60.0e0 + 8.40 / 60e0 / 60e0;
  const double latMm_b_su_genova = 41 + 55 / 60.0e0 + 24.399 / 60e0 / 60e0, lonMm_b_su_genova = 3 + 31e0 / 60.0e0 + 51.131 / 60e0 / 60e0;
  CassiniSoldner projcs(latMm_b_su_genova, lonMm_b_su_genova, Bessel);
  CassiniSoldner projhy(latMm, lonMm, Hayford);
  double lat, lon;
  projcs.Reverse(0e0,10000.0e0, lat, lon);
  dasdas(lat);
  dasdas(lon);
  double s12, az1, az2;
  double arc = Bessel.Inverse(latMm_b_su_genova, lonMm_b_su_genova, lat, lon, s12, az1, az2);
  dasdas(lat);
  dasdas(lon);
  Hayford.Direct(latMm, lonMm, az1, s12, lat, lon);
  //riduzione alla corda
  controllo.NEgb(41.552551,12.270840, FALSE);
  double Norde = controllo.NordFE; double Este = controllo.EstFE-2520000.0e0;
  controllo.NEgb(dasdas(lat), dasdas(lon), FALSE);
  double eps12 = (controllo.NordFE - Norde) * (2 * (controllo.EstFE - 2520000.0) + Este) / (6 * controllo.ro * controllo.GN);
  double teta, ss, cd, sd, m, deltaalfa;
  angdor(0e0, 0e0, 0e0, 10000.0e0, ss, teta, cd, sd, m, deltaalfa);
  az1 = az1 + (eps12 + deltaalfa) * 180 / PI;
  Hayford.Direct(latMm, lonMm, az1, s12 / m, lat, lon);
  controllo.NEgb(dasdas(lat), dasdas(lon), FALSE);*/
    double n, e;
    e = estCS;
    n = nordCS;

    


    char rr[300];
    double d1, d2;
    char mms[50];

 
   
    //geo.FiLagb(4368144.77, 2692758.93, latMm, lonMm);
    //latMm = daras(latMm); lonMm = daras(lonMm);
    /*
    double Nordo = geo.NordFO;
    double Esto = geo.EstFO-1500000.0;
    double Norde = geo.NordFE;
    double Este = geo.EstFE-2520000.0;
    //convergenza del meridiano
    double fi0rad = dasar(41.552551);
    double la0rad = dasar(12.27040-15e0);
    double sfi = sin(fi0rad);
    double cfi = cos(fi0rad);
    double gamma = -la0rad * sfi + (pow(la0rad, 3) / 3e0) * (sfi * pow(cfi, 2) * (1+0.02030*pow(cfi,2)));
    //gamma = darag(gamma) * 0.9e0;
    {
      double s12, az1, az2;
      double lat, lon;
      projcs.Reverse(50000.0e0, 50000.0e0, lat, lon,az1,az2);


    double arc = Bessel.Inverse(latMm_b_su_genova, lonMm_b_su_genova, lat, lon, s12, az1, az2);
    dasdas(az1);
    }*/
    Geodesia gi;
    
    //settiamo ellissoide di bessel
    gi.SetAsse(6377397.155000);
    gi.SetEcc(6.674372230000e-3);
    const double latMm = 41e0 + 55e0 / 60.0e0 + 25.51e0 / 60e0 / 60e0, lonMm = 12e0 + 27e0 / 60.0e0 + 8.40e0 / 60e0 / 60e0;
    const double latMm_b_su_genova = 41e0 + 55e0 / 60.0e0 + 24.399e0 / 60e0 / 60e0, lonMm_b_su_genova = 3e0 + 31e0 / 60.0e0 + 51.131 / 60e0 / 60e0;
    gi.calcola(i_u_d, latMm_b_su_genova, lonMm_b_su_genova);
    // gi.calcola(42.071627, 0.000000);
    double N0 = gi.GN;
    double N02 = N0 * N0;
    double Ro0 = gi.ro;
    double R0 = gi.r;
    double la_0 = gi._lambda;
    double fi_0 = gi.fi;
    double semiasse = gi.semiasse;
    double semiasse2 = semiasse * semiasse;
    double ecc = gi.ecc;
    double lat_mia , lon_mia , az1_mia, az2_mia;
    gi.calcola(i_u_d, latMm_b_su_genova, lonMm_b_su_genova);
    //trasporto geografiche problema diretto 
    if (n > 0)
    {
      lat_mia = gi.fis(n, 0e0, i_u_r);
      lon_mia = gi.las(n, 0e0, i_u_r);
    }
    else
    {
      lat_mia = gi.fis(-n, PI, i_u_r);
      lon_mia = gi.las(-n, PI, i_u_r);
    }
    double lat_mia0 = lat_mia;
    double lon_mia0 = lon_mia;
    //settiamo ellissoide di bessel
    gi.calcola(i_u_r, lat_mia, lon_mia);
    if (e > 0)
    {
      lon_mia = gi.las(e, PI / 2, i_u_r);
      lat_mia = gi.fis(e, PI / 2, i_u_r);
    }
    else
    {
      lon_mia = gi.las(-e, 3 * PI / 2, i_u_r);
      lat_mia = gi.fis(-e, 3 * PI / 2, i_u_r);
    }


    gi.calcola(i_u_r, lat_mia, lon_mia); //entra radianti
    //gi.calcola(43.014487, 0.362548);

    double d_f = gi.fi - fi_0;
    double d_l = gi._lambda - la_0;
    //d_f -=fi_0;
    //d_l -=la_0;

    double p_p = d_f;
    p_p += (3e0 * ecc * N02 * sin(2e0 * fi_0)) / (4e0 * semiasse2) * d_f * d_f;
    p_p += N0 * sin(2e0 * fi_0) / (4e0 * Ro0) * d_l * d_l;
    p_p += (ecc * N02 / (2e0 * semiasse2)) * ((5e0 * ecc * N02 * pow(sin(2e0 * fi_0), 2e0)) / (4e0 * semiasse2) + cos(2e0 * fi_0)) * pow(d_f, 3e0);
    p_p += (R0 * cos(fi_0) / (6e0 * Ro0) - pow(sin(fi_0), 2e0) / 2e0) * d_f * d_l * d_l;
    double q_q = d_l;
    q_q -= Ro0 * sin(fi_0) * d_l * d_f / R0;
    q_q -= Ro0 * ((3e0 * ecc * N02 * pow(sin(fi_0), 2e0)) / (semiasse2)+2.0e0 / 3.0e0) / (2e0 * N0) * d_f * d_f * d_l;
    q_q -= pow(sin(fi_0), 2e0) * pow(d_l, 3e0) / 6.0e0;
    double azimutgeodetica = atan2((q_q * R0), (p_p * Ro0));

    if (azimutgeodetica < 0) azimutgeodetica += 2e0 * PI;
    double arcogeodeticab = p_p * Ro0 / cos(azimutgeodetica);
    double arcogeodeticac = q_q * R0 / sin(azimutgeodetica);
    double arcogeodetica = (arcogeodeticab + arcogeodeticac) / 2;
    // double azses = daras(azimutgeodetica);
    azimutgeodetica = (azimutgeodetica * 180 / PI);






    double s12, az1, az2;
    


  
    
    gi.SetAsse(6377397.155000);
    gi.SetEcc(6.674372230000e-3);
    double arco_bessel; double az1_bessel; double az2_bessel;
    double arco_hayford; double az1_hayford; double az2_hayford;
    double lat_bessel; double lon_bessel;
    double lat_hayford; double lon_hayford;
    gi.DaCSaGB(latMm_b_su_genova, lonMm_b_su_genova,
      n, e, arco_bessel, az1_bessel, az2_bessel,
      arco_hayford, az1_hayford, az2_hayford,
      lat_bessel, lon_bessel,
      lat_hayford, lon_hayford, i_u_d);
    lat_bessel = daras(lat_bessel);
    lon_bessel = daras(lon_bessel);
    lat_hayford = daras(lat_hayford);
    lon_hayford = daras(lon_hayford);

    az1_bessel = daras(az1_bessel);
    az2_bessel = daras(az2_bessel);
    az1_hayford = daras(az1_hayford);
    az2_hayford = daras(az2_hayford);


    Geodesia geo;


      cout.width(10);
      cout.precision(8);
      loGB = lon_hayford;
      fiGB = lat_hayford;
      loCS = lon_bessel;
      fiCS = lat_bessel;
      geo.NEgb(lat_hayford, lon_hayford, false);
      nordGBFE = geo.NordFE; estGBFE = geo.EstFE;
      nordGBFO = geo.NordFO; estGBFO = geo.EstFO;
#ifdef _DEBUG
      cout <<  " " << fixed << " nord " << n << " est " << e << " lat " << lat_bessel << " lon " << lon_bessel<< endl;
      cout << "                                                   lat " << lat_hayford << " lon " << lon_hayford << endl;
 
      cout << " Nord Fuso Ovest " << geo.NordFO << " Est Fuso Ovest " << geo.EstFO << endl;
      cout << " Nord Fuso Est   " << geo.NordFE << " Est Fuso Est   " << geo.EstFE << endl;
      cout << "----------------------------------------------------------------------------------------------------------------------------------" << endl;
      cout << fixed << " nord " << n << " est " << e << " lat " << lat_bessel << " lon " << lon_bessel << " s0i " << arco_bessel << " az1 " << az1_bessel << " az2 " << az2_bessel << endl;
      cout << "                                                   lat " << lat_hayford << " lon " << lon_hayford << " s0h " << arco_hayford << " az1 " << az1_hayford << " az2 " << az2_hayford << endl;
      geo.NEgb((lat_hayford), (lon_hayford), false);
      cout << " Nord Fuso Ovest " << geo.NordFO << " Est Fuso Ovest " << geo.EstFO << endl;
      cout << " Nord Fuso Est   " << geo.NordFE << " Est Fuso Est   " << geo.EstFE << endl;
      cout << "----------------------------------------------------------------------------------------------------------------------------------" << endl;
      char rs[400];
      sprintf(rs, "8|%s|%11.3f|%11.3f|%11.3f|%11.3f|", "punto", geo.NordFE, geo.EstFE, geo.NordFO, geo.EstFO);
      cout << rs << endl;
#endif


    //prova su lecce
#ifdef _LECCE
    {
      //const double latMm = 41 + 55 / 60.0e0 + 25.51 / 60e0 / 60e0, lonMm = 12 + 27e0 / 60.0e0 + 8.40 / 60e0 / 60e0;
      //const double latMm_b_su_genova = 41 + 55 / 60.0e0 + 24.399 / 60e0 / 60e0, lonMm_b_su_genova = 3 + 31e0 / 60.0e0 + 51.131 / 60e0 / 60e0;
      const double latMm_h_su_Lecce = 40 + 21 / 60.0e0 + 4.7395 / 60e0 / 60e0, lonMm_h_su_Lecce = 18 + 10e0 / 60.0e0 + 9.1084 / 60e0 / 60e0;
      dasdas(latMm_h_su_Lecce); dasdas(lonMm_h_su_Lecce);
      const double latMm_b_su_Lecce = 40 + 21 / 60.0e0 + 2.454 / 60e0 / 60e0, lonMm_b_su_Lecce = 9 + 14e0 / 60.0e0 + 54.917 / 60e0 / 60e0;
      double x = 1000e0; double y = 1000e0;
      CassiniSoldner projcs(latMm_b_su_Lecce, lonMm_b_su_Lecce, Bessel);
      Geodesia geo;
      geo.calcola(); //siamo su hayford
      geo.NEgb(dasdas(latMm_h_su_Lecce), dasdas(lonMm_h_su_Lecce), FALSE);
      double lat, lon, s12, az1, az2;
      projcs.Reverse(x, y, lat, lon);
      double arc = Bessel.Inverse(latMm_b_su_Lecce, lonMm_b_su_Lecce, lat, lon, s12, az1, az2);
      clat = dasdas(lat);
      clon = dasdas(lon);
      caz1 = dasdas(az1);
      caz2 = dasdas(az2);
      double cs12 = s12;
      az1 += (0.88e0 - 4.96e0) / 60e0 / 60e0;
      Hayford.Direct(latMm_h_su_Lecce, lonMm_h_su_Lecce, az1, s12, lat, lon);
      lat = dasdas(lat);
      lon = dasdas(lon);
      az1 = dasdas(az1);
      az2 = dasdas(az2);
      geo.NEgb(lat, lon, FALSE);


      cout << setprecision(8);
      cout << setw(10);
      cout << fixed << "nord " << y << " est " << x << endl << " lat " << clat << " lon " << clon << " s0i " << cs12 << " az12 " << caz1 << " az21 " << caz2 << endl;
      cout << "lat " << lat << " lon " << lon << " s0h " << s12 << " az12 " << az1 << " az21 " << az1 << endl;


      cout << " Nord Fuso Ovest " << geo.NordFO << " Est Fuso Ovest " << geo.EstFO << endl;
      cout << " Nord Fuso Est   " << geo.NordFE << " Est Fuso Est   " << geo.EstFE << endl;
      cout << " ________________________________________________________________________________" << endl;



    }
#endif



}
void calcolo(_POINT &p,bool utm)
{
/*
Geodesia controllo;
controllo.NEgb(0e0, 15e0, FALSE);
Geodesic Bessel(6377397.15, 1.0e0 / 299.15);
Geodesic Hayford(6378388.0e0, 1.0e0 / 297.0);
const double latMm = 41 + 55 / 60.0e0 + 25.51 / 60e0 / 60e0, lonMm = 12 + 27e0 / 60.0e0 + 8.40 / 60e0 / 60e0;
const double latMm_b_su_genova = 41 + 55 / 60.0e0 + 24.399 / 60e0 / 60e0, lonMm_b_su_genova = 3 + 31e0 / 60.0e0 + 51.131 / 60e0 / 60e0;
CassiniSoldner projcs(latMm_b_su_genova, lonMm_b_su_genova, Bessel);
CassiniSoldner projhy(latMm, lonMm, Hayford);
double lat, lon;
projcs.Reverse(0e0,10000.0e0, lat, lon);
dasdas(lat);
dasdas(lon);
double s12, az1, az2;
double arc = Bessel.Inverse(latMm_b_su_genova, lonMm_b_su_genova, lat, lon, s12, az1, az2);
dasdas(lat);
dasdas(lon);
Hayford.Direct(latMm, lonMm, az1, s12, lat, lon);
//riduzione alla corda
controllo.NEgb(41.552551,12.270840, FALSE);
double Norde = controllo.NordFE; double Este = controllo.EstFE-2520000.0e0;
controllo.NEgb(dasdas(lat), dasdas(lon), FALSE);
double eps12 = (controllo.NordFE - Norde) * (2 * (controllo.EstFE - 2520000.0) + Este) / (6 * controllo.ro * controllo.GN);
double teta, ss, cd, sd, m, deltaalfa;
angdor(0e0, 0e0, 0e0, 10000.0e0, ss, teta, cd, sd, m, deltaalfa);
az1 = az1 + (eps12 + deltaalfa) * 180 / PI;
Hayford.Direct(latMm, lonMm, az1, s12 / m, lat, lon);
controllo.NEgb(dasdas(lat), dasdas(lon), FALSE);*/
double n, e;
/*calcola gli elementi che non sono VALORE_NUMERICO_NULLO*/
  if (utm == false)
  {
    e = p.estcs;
    n = p.nordcs;

    char rr[300];
    double d1, d2;
    char mms[50];
    Geodesia gi;

    //settiamo ellissoide di bessel
    gi.SetAsse(6377397.155000);
    gi.SetEcc(6.674372230000e-3);
    const double latMm = 41e0 + 55e0 / 60.0e0 + 25.51e0 / 60e0 / 60e0, lonMm = 12e0 + 27e0 / 60.0e0 + 8.40e0 / 60e0 / 60e0;
    const double latMm_b_su_genova = 41e0 + 55e0 / 60.0e0 + 24.399e0 / 60e0 / 60e0, lonMm_b_su_genova = 3e0 + 31e0 / 60.0e0 + 51.131 / 60e0 / 60e0;
    gi.calcola(i_u_d, latMm_b_su_genova, lonMm_b_su_genova);
    // gi.calcola(42.071627, 0.000000);
    double N0 = gi.GN;
    double N02 = N0 * N0;
    double Ro0 = gi.ro;
    double R0 = gi.r;
    double la_0 = gi._lambda;
    double fi_0 = gi.fi;
    double semiasse = gi.semiasse;
    double semiasse2 = semiasse * semiasse;
    double ecc = gi.ecc;
    double lat_mia, lon_mia, az1_mia, az2_mia;
    gi.calcola(i_u_d, latMm_b_su_genova, lonMm_b_su_genova);
    //trasporto geografiche problema diretto 
    if (n > 0)
    {
      lat_mia = gi.fis(n, 0e0, i_u_r);
      lon_mia = gi.las(n, 0e0, i_u_r);
    }
    else
    {
      lat_mia = gi.fis(-n, PI, i_u_r);
      lon_mia = gi.las(-n, PI, i_u_r);
    }
    double lat_mia0 = lat_mia;
    double lon_mia0 = lon_mia;
    //settiamo ellissoide di bessel
    gi.calcola(i_u_r, lat_mia, lon_mia);
    if (e > 0)
    {
      lon_mia = gi.las(e, PI / 2, i_u_r);
      lat_mia = gi.fis(e, PI / 2, i_u_r);
    }
    else
    {
      lon_mia = gi.las(-e, 3 * PI / 2, i_u_r);
      lat_mia = gi.fis(-e, 3 * PI / 2, i_u_r);
    }
    gi.calcola(i_u_r, lat_mia, lon_mia); //entra radianti
    double d_f = gi.fi - fi_0;
    double d_l = gi._lambda - la_0;
    //d_f -=fi_0;
    //d_l -=la_0;

    double p_p = d_f;
    p_p += (3e0 * ecc * N02 * sin(2e0 * fi_0)) / (4e0 * semiasse2) * d_f * d_f;
    p_p += N0 * sin(2e0 * fi_0) / (4e0 * Ro0) * d_l * d_l;
    p_p += (ecc * N02 / (2e0 * semiasse2)) * ((5e0 * ecc * N02 * pow(sin(2e0 * fi_0), 2e0)) / (4e0 * semiasse2) + cos(2e0 * fi_0)) * pow(d_f, 3e0);
    p_p += (R0 * cos(fi_0) / (6e0 * Ro0) - pow(sin(fi_0), 2e0) / 2e0) * d_f * d_l * d_l;
    double q_q = d_l;
    q_q -= Ro0 * sin(fi_0) * d_l * d_f / R0;
    q_q -= Ro0 * ((3e0 * ecc * N02 * pow(sin(fi_0), 2e0)) / (semiasse2)+2.0e0 / 3.0e0) / (2e0 * N0) * d_f * d_f * d_l;
    q_q -= pow(sin(fi_0), 2e0) * pow(d_l, 3e0) / 6.0e0;
    double azimutgeodetica = atan2((q_q * R0), (p_p * Ro0));

    if (azimutgeodetica < 0) azimutgeodetica += 2e0 * PI;
    double arcogeodeticab = p_p * Ro0 / cos(azimutgeodetica);
    double arcogeodeticac = q_q * R0 / sin(azimutgeodetica);
    double arcogeodetica = (arcogeodeticab + arcogeodeticac) / 2;
    // double azses = daras(azimutgeodetica);
    azimutgeodetica = (azimutgeodetica * 180 / PI);
    double s12, az1, az2;
    gi.SetAsse(6377397.155000);
    gi.SetEcc(6.674372230000e-3);
    double arco_bessel; double az1_bessel; double az2_bessel;
    double arco_hayford; double az1_hayford; double az2_hayford;
    double lat_bessel; double lon_bessel;
    double lat_hayford; double lon_hayford;
    gi.DaCSaGB(latMm_b_su_genova, lonMm_b_su_genova,
      n, e, arco_bessel, az1_bessel, az2_bessel,
      arco_hayford, az1_hayford, az2_hayford,
      lat_bessel, lon_bessel,
      lat_hayford, lon_hayford, i_u_d);
    lat_bessel = daras(lat_bessel);
    lon_bessel = daras(lon_bessel);
    lat_hayford = daras(lat_hayford);
    lon_hayford = daras(lon_hayford);

    az1_bessel = daras(az1_bessel);
    az2_bessel = daras(az2_bessel);
    az1_hayford = daras(az1_hayford);
    az2_hayford = daras(az2_hayford);


    Geodesia geo;
    p.logb = dasasd(lon_hayford);
    p.figb = dasasd(lat_hayford);
    p.locs = dasasd(lon_bessel);
    p.fics = dasasd(lat_bessel);
  
    geo.NEgb(lat_hayford, lon_hayford, false);
    p.nordgbFE = geo.NordFE; p.estgbFE = geo.EstFE;
    p.nordgbFO = geo.NordFO; p.estgbFO = geo.EstFO;
  }
  else
  {
    Geodesia geo;
    geo.SetEllWgs84();
    int fuso = 0;
    geo.NEutm(p.fi2000, p.lo2000, fuso);
    p.nordUTM = geo.NordUTM;
    p.estUTM = geo.EstUTM;
    p.fuso = fuso;
  }
}

 
string removfirstblanck(string a)
{
  auto pos = a.find(' ');
  while (pos != string::npos)
  {
    a.erase(pos, 1);
    pos = a.find(' ');
  }
  return a;
}


bool  calcolaunpunto(GRI_HDR* hh, double nordcs, double estcs, double& nordutm, double& estutm, int& fuso)
{
 
  if (hh ==GRI_NULL)
  {
    if (nomefilegriglia.empty())
    {
      return false;
    }
    int* prc=NULL;
    hh = gri_load_file(nomefilegriglia.c_str(), false, true, NULL, prc);
    if (hh->fp->fail())
    {
      return false;
    }
  }
  _POINT p;
  p.estcs = estcs;
  p.nordcs = nordcs;
  calcolo(p);
  double fi_ingr = p.figb;
  double lo_ingr = p.logb;
  process_lat_lon(hh, fi_ingr, lo_ingr);
  p.fi2000 = dasdas(fi_ingr);
  p.lo2000 = dasdas(lo_ingr);
  calcolo(p, true);
  p.fi2000 = dasdas(fi_ingr);
  p.lo2000 = dasdas(lo_ingr);
  calcolo(p, true);
  nordutm = p.nordUTM;
  estutm = p.estUTM;
  fuso = p.fuso;
  return true;
}
