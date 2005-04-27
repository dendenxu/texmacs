
/******************************************************************************
* MODULE     : curve.cpp
* DESCRIPTION: mathematical curves
* COPYRIGHT  : (C) 2003  Joris van der Hoeven and Henri Lesourd
*******************************************************************************
* This software falls under the GNU general public license and comes WITHOUT
* ANY WARRANTY WHATSOEVER. See the file $TEXMACS_PATH/LICENSE for more details.
* If you don't have this file, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
******************************************************************************/

#include "curve.hpp"
#include "frame.hpp"
#include "equations.hpp"
#include "math_util.hpp"

/******************************************************************************
* General routines
******************************************************************************/

array<point>
curve_rep::rectify (double err) {
  array<point> a (1);
  a[0]= evaluate (0.0);
  rectify_cumul (a, err);
  return a;
}

/******************************************************************************
* Segments
******************************************************************************/

struct segment_rep: public curve_rep {
  point p1, p2;
  segment_rep (point p1b, point p2b): p1 (p1b), p2 (p2b) {}
  point evaluate (double t) { return (1.0-t)*p1 + t*p2; }
  void rectify_cumul (array<point>& a, double err) { a << p2; }
};

curve
segment (point p1, point p2) {
  return new segment_rep (p1, p2);
}

/******************************************************************************
* Poly-segments
******************************************************************************/

struct poly_segment_rep: public curve_rep {
  array<point> a;
  int n;
  poly_segment_rep (array<point> a2): a (a2), n(N(a)-1) {}
  int nr_components () { return n; }
  point evaluate (double t) {
    int i= min ((int) (n*t), n-1);
    return (1.0-t)*a[i] + t*a[i+1];
  }
  void rectify_cumul (array<point>& cum, double err) {
    int i;
    for (i=0; i<n; i++)
      cum << a[i+1];
  }
};

curve
poly_segment (array<point> a) {
  return new poly_segment_rep (a);
}

/******************************************************************************
* Splines
******************************************************************************/

static const double epsilon=0.01;//0.00005;

struct spline_rep: public curve_rep {
  array<point> a;
  int n;
  array<double> U;
  array<polynomials> p;

  spline_rep (array<point> a2, bool close=false, bool interpol=true);

  inline double d (int i,int k) { return U[i]-U[i-k]; }
  inline double m (int i) { return (U[i]+U[i+1])/2; }

  double convert (double u) { return U[2]+u*(U[n+1]-U[2]); }
  int interval_no (double u);

  point spline (int i,double u,int o=0);
  inline double S (
    array<polynomial> p1, array<polynomial> p2, array<polynomial> p3,
    int i, double u);
  point evaluate (double t,int o);
  point evaluate (double t);
  double bound (double t, double err);
  point grad (double t, bool& error);

  double curvature (int i, double t1, double t2);
  double curvature (double t1, double t2);

  bool approx (int i,double u1,double u2,double err);
  void rectify_cumul (array<point>& cum, int i,
		      double u1, double u2, double err);
  void rectify_cumul (array<point>& cum, double err);
};

// Creation
spline_rep::spline_rep (array<point> a2, bool close,bool interpol):
  a(a2), n(N(a)-1)
{
  array<polynomial> p1,p2,p3;
  p1= array<polynomial> (n+3);
  p2= array<polynomial> (n+3);
  p3= array<polynomial> (n+3);
  p = array<polynomials> (n+3);
  U = array<double> (n+6);
  if (close) {
    int i;
    a->resize(N(a)+2);
    for (i=0;i<=1;i++)
      a[n+1+i]=a[i];
    n+=2;
  }
  int i;
  double x=0;
  if (!close) {
    for (i=0;i<3;i++) {
      U[i]=x;
      x+=epsilon; 
    }
    x+=1-epsilon;
    for (i=3;i<=n;i++) {
      U[i]=x;
      x+=1;
    }
    for (i=n+1;i<=n+3;i++) {
      U[i]=x;
      x+=epsilon;
    }
  }
  else {
    for (i=0;i<=n+3;i++) {
      U[i]=x;
      x+=1;
    }
  }
  for (i=0;i<=n;i++) {
    double di22= d(i+2,2);
    double di11= d(i+1,1);
    double di21= d(i+2,1);
    double di32= d(i+3,2);
    double di31= d(i+3,1);
    p1[i]= polynomial(2);
    p2[i]= polynomial(2);
    p3[i]= polynomial(2);
    p1[i][2]= 1/di22/di11;
    p1[i][1]= -2*U[i]/di22/di11;
    p1[i][0]= square(U[i])/di22/di11;
    p2[i][2]= -1/di22/di21-1/di32/di21;
    p2[i][1]= (U[i+2]+U[i])/di22/di21+(U[i+3]+U[i+1])/di32/di21;
    p2[i][0]= -U[i+2]*U[i]/di22/di21-U[i+3]*U[i+1]/di32/di21;
    p3[i][2]= 1/di32/di31;
    p3[i][1]= -2*U[i+3]/di32/di31;
    p3[i][0]= square(U[i+3])/di32/di31;
  }
  if (interpol) {
    array<point> x(n+1), y(n+1);
    y= a;
    {
      array<double> a(n+1), b(n+1), c(n+1);
      if (close) {
        a[n-2]= S (p1, p2, p3, n-3, m(n-1));
        b[0]= S (p1, p2, p3, 1, m(2));
        b[n-2]= S (p1, p2, p3, n-2, m(n-1));
        c[0]=S (p1, p2, p3, 2, m(2));
        for (i=1; i<n-2; i++) {
           a[i]= S (p1, p2, p3, i, m(i+2));
           b[i]= S (p1, p2, p3, i+1, m(i+2));
           c[i]= S (p1, p2, p3, i+2, m(i+2));
        }
        xtridiag_solve (a, b, c, S(p1, p2, p3, n-1, m(n-1)),
                                 S(p1, p2, p3, 0, m(2)),
                                 x, y, n-1);
        x[n-1]= x[0];
        x[n]= x[1];
      }
      else {
        a[n]= S (p1, p2, p3, n-1, U[n+1]);
        b[0]= S (p1, p2, p3, 0, U[2]);
        b[n]= S (p1, p2, p3, n, U[n+1]);
        c[0]= S (p1, p2, p3, 1, U[2]);
        for (i=1; i<n; i++) {
           a[i]= S (p1, p2, p3, i-1, m(i+1));
           b[i]= S (p1, p2, p3, i, m(i+1));
           c[i]= S (p1, p2, p3, i+1, m(i+1));
        }
        tridiag_solve (a, b, c, x, y, n+1);
      }
    }
    a= x;
  }
  for (i=2; i<=n; i++)
    p[i]= a[i]*p1[i] + a[i-1]*p2[i-1] + a[i-2]*p3[i-2];
}

// Evaluation
double
spline_rep::S (
  array<polynomial> p1, array<polynomial> p2, array<polynomial> p3,
  int i, double u)
{
  if (i<0 || i>n) return 0;
  else if (u<U[i] || u>=U[i+3]) return 0;
  else if (u<U[i+1]) return p1[i](u);
  else if (u<U[i+2]) return p2[i](u);
  else if (u<U[i+3]) return p3[i](u);
  // FIXME: and otherwise ?
}

point
spline_rep::spline (int i,double u,int o) {
  point res;
  if (o<0 || o>2) o=0;
  res=p[i](u,o);
  return res;
}
int
spline_rep::interval_no (double u) {
  int i;
  for (i=0;i<N(U);i++)
    if (u>=U[i] && u<U[i+1]) return i;
  return -1;
}

point
spline_rep::evaluate (double t,int o) {
  point res;
  int no;
  t=convert(t);
  no=interval_no(t);
  if (no<2) res=spline(2,U[2],o);
  else if (no>n) res=spline(n,U[n+1],o);
  else res=spline(no,t,o);
  return res;
}

point
spline_rep::evaluate (double t) {
  return evaluate(t,0);
}

double
spline_rep::bound (double t, double err) {
  return err/norm(evaluate(t,1));
}

point
spline_rep::grad (double t, bool& error) {
  error= false;
  return evaluate(t,1);
}

// Rectification
bool
spline_rep::approx (int i,double u1,double u2,double err) {
  double l,R;
  point p1,p2;
  p1=spline(i,u1);
  p2=spline(i,u2);
  l=norm(p1-p2);
// When l and R are very small, the test l<=R
// can fail forever. So we set l to exactly 0
  if (l!=0 && fnull (l,1.0e-6)) l=0;
  R=curvature(i,u1,u2);
  return l<=2*sqrt(2*R*err);
}

void
spline_rep::rectify_cumul (array<point>& cum, int i,
                           double u1, double u2, double err) {
  if (approx(i,u1,u2,err))
    cum << spline(i,u2);
  else {
    double u=(u1+u2)/2;
    rectify_cumul(cum,i,u1,u,err);
    rectify_cumul(cum,i,u,u2,err);
  }
}

void
spline_rep::rectify_cumul (array<point>& cum, double err) {
  int i;
  for (i=2;i<=n;i++)
    rectify_cumul(cum,i,U[i],U[i+1],err);
}

// Curvature
double
spline_rep::curvature (int i, double t1, double t2) {
  point a,b;
  a=coeffs(p[i],2);
  b=coeffs(p[i],1);
  double t,R;
  point pp,ps;
  if (norm(a)==0) return tm_infinity;
  t=-(a*b)/(2*a*a);
  if (t1>t) t=t1;
  else if (t2<t) t=t2;
  pp=spline(i,t,1);
  ps=spline(i,t,2);
  if (norm(ps)==0) return tm_infinity;
  R=square(norm(pp))/norm(ps);
  return R;
}

double
spline_rep::curvature (double t1, double t2) {
  double res;
  int no,no1,no2;
  t1=convert(t1);
  t2=convert(t2);
  no1=interval_no(t1);
  no2=interval_no(t2);
  res=tm_infinity;
  for (no=no1;no<=no2;no++) {
    double r;
    if (no<2) r=curvature(2,t1,t2);
    else if (no>n) r=curvature(n,t1,t2);
    else r=curvature(no,t1,t2);
    if (r<=res) res=r;
  }
  return res;
}

curve
spline (array<point> a, bool close, bool interpol) {
  return new spline_rep (a, close, interpol);
}

/******************************************************************************
* Arcs
******************************************************************************/

struct arc_rep: public curve_rep {
  point center;  // Center
  double r1, r2; // The two radiuses of the ellipsis
  double alpha;  // Angle to define how the ellipsis is rotated
  double e1, e2; // Coordinates of the two extremal points of the arc
  arc_rep (
    point c, double r1b, double r2b, double a, double e1b, double e2b):
    center (c), r1 (r1b), r2 (r2b), alpha (a), e1 (e1b), e2 (e2b-e1b) {}
  point evaluate (double t);
  void rectify_cumul (array<point>& cum, double err);
  double bound (double t, double err);
  point grad (double t, bool& error);
  double curvature (double t1, double t2);
};

point
arc_rep::evaluate (double t) {
  return center + r1*cos(2*tm_PI*(e1+t))*point (cos(alpha), sin(alpha))
                + r2*sin(2*tm_PI*(e1+t))*point (-sin(alpha), cos(alpha));
}

void
arc_rep::rectify_cumul (array<point>& cum, double err) {
  double t, step;
  step= sqrt (2*err / max (r1, r2) ) / tm_PI;
  for (t=step; t<=e2; t+=step)
    cum << evaluate (t);
  if (t-step != e2)
    cum << evaluate (e2);
}

double
arc_rep::bound (double t, double err) {
  bool b;
  return err/norm(grad(t,b));
}

point
arc_rep::grad (double t, bool& error) {
  error= false;
  return -2*tm_PI*r1*sin(2*tm_PI*(e1+t))*point (cos(alpha), sin(alpha))
         +2*tm_PI*r2*cos(2*tm_PI*(e1+t))*point (-sin(alpha), cos(alpha));
}

double
arc_rep::curvature (double t1, double t2) {
  if (r1 >= r2)
    return fnull (r1,1.0e-6) ? tm_infinity : square(r2)/r1;
  else
    return fnull (r2,1.0e-6) ? tm_infinity : square(r1)/r2;
}

curve
arc (point center, double r1, double r2, double alpha, double e1, double e2) {
  return new arc_rep (center, r1, r2, alpha, e1, e2);
}

/******************************************************************************
* Compound curves
******************************************************************************/

struct compound_curve_rep: public curve_rep {
  curve c1, c2;
  int n1, n2;
  compound_curve_rep (curve c1b, curve c2b):
    c1 (c1b), c2 (c2b), n1 (c1->nr_components()), n2 (c2->nr_components()) {}
  int nr_components () { return n1 + n2; }
  point evaluate (double t) {
    double n= n1+n2;
    if (t <= n1/n) return c1 (t*n/n1);
    else return c2 (t*n/n2 - n1); }
  void rectify_cumul (array<point>& a, double err) {
    c1->rectify_cumul (a, err);
    c2->rectify_cumul (a, err);
  }
};

curve
operator * (curve c1, curve c2) {
  // FIXME: we might want to test whether c1(1.0) is approx equal to c2(0.0)
  return new compound_curve_rep (c1, c2);
}

/******************************************************************************
* Inverted curves
******************************************************************************/

struct inverted_curve_rep: public curve_rep {
  curve c;
  int n;
  inverted_curve_rep (curve c2): c (c2), n (c->nr_components()) {}
  int nr_components () { return n; }
  point evaluate (double t) { return c (1.0 - t); }
  void rectify_cumul (array<point>& a, double err) {
    array<point> b= c->rectify (err);
    int i, k= N(b);
    for (i=k-1; i>=0; i--) a << b[i];
  }
};

curve
invert (curve c) {
  return new inverted_curve_rep (c);
}

/******************************************************************************
* Transformed curves
******************************************************************************/

struct transformed_curve_rep: public curve_rep {
  frame f;
  curve c;
  int n;
  transformed_curve_rep (frame f2, curve c2):
    f (f2), c (c2), n (c->nr_components()) {}
  int nr_components () { return n; }
  point evaluate (double t) { return f (c (t)); }
  void rectify_cumul (array<point>& a, double err);
};

void
transformed_curve_rep::rectify_cumul (array<point>& a, double err) {
  if (f->linear) {
    double delta= f->direct_bound (c(0.0), err);
    array<point> b= c->rectify (delta);
    int i, k= N(b);
    for (i=0; i<k; i++) a << f(b[i]);
  }
  else fatal_error ("Not yet implemented",
		    "transformed_curve_rep::rectify_cumul");
}

curve
frame::operator () (curve c) {
  return new transformed_curve_rep (*this, c);
}

curve
frame::operator [] (curve c) {
  return new transformed_curve_rep (invert (*this), c);
}
