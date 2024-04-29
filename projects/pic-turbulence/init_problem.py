
#from configSetup import Configuration #old conf
from pytools import Configuration # new conf
import pytools  # runko python tools

from pytools import simplify_string, simplify_large_num

from numpy import sqrt, pi
import numpy as np


class Configuration_Turbulence(Configuration):

    def __init__(self, *file_names, do_print=False):
        Configuration.__init__(self, *file_names)

        #--------------------------------------------------
        # problem specific initializations
        if do_print:
            print("init:--------------------------------------------------")
            print("init: Initializing turbulence setup...")
            print("init: conf file:", file_names[0])

        #--------------------------------------------------
        # create directory naming scheme
        if self.outdir == "auto":

            # base name
            self.outdir = self.prefix if "prefix" in self.__dict__ else ""

            # grid size
            str_x = str(int(self.Nx))
            str_m = str(int(self.NxMesh))
            self.outdir += "x" + str_x + "m" + str_m + "_"

            #--------------------------------------------------
            # ppc
            str_ppc  = simplify_string(self.ppc)
            str_np   = simplify_string(self.npasses)
            str_comp = simplify_string(self.c_omp)
            self.outdir += "p" + str_ppc + "np" + str_np + "c" + str_comp + "_"

            #--------------------------------------------------
            # sigma
            str_sig = simplify_string(self.sigma)
            str_delg = simplify_string(self.delgam)
            self.outdir += "s" + str_sig + "d" + str_delg + "_"

            #--------------------------------------------------
            # radiation 
            if "gammarad" in self.__dict__:
                if self.gammarad > 0.0:
                    str_r = str(int(self.gammarad))
                    self.outdir += "r" + str_r + "_"

            #--------------------------------------------------
            # driving  amplitude
            if "drive_ampl" in self.__dict__:
                str_a = simplify_string(self.drive_ampl)
                self.outdir += "a" + str_a

            if "drive_freq" in self.__dict__:
                str_f = simplify_string(self.drive_freq)
                self.outdir += "w" + str_f

            if "decorr_time" in self.__dict__:
                str_d = simplify_string(self.decorr_time)
                self.outdir += "g" + str_d 

            #--------------------------------------------------
            # postfix string
            if "postfix" in self.__dict__:
                self.outdir += self.postfix

        if do_print:
            print("init: output dir", self.outdir)


        #--------------------------------------------------
        # turn off some advanced functionality
        self.use_injector = False
        self.c_corr = 1.0 # no speed of light correction
        self.use_maxwell_split = False

        # local variables just for easier/cleaner syntax
        me = np.abs(self.me)
        mi = np.abs(self.mi)
        cfl = self.cfl
        ppc = self.ppc * 2.0  # multiply x2 to account for 2 species/pair plasma

        # plasma reaction & subsequent normalization
        self.gamma = 1.0
        self.omp = cfl / self.c_omp
        self.qe = -(self.omp ** 2.0 * self.gamma) / ((ppc * 0.5) * (1.0 + me / mi))
        self.qi = -self.qe

        me *= abs(self.qi)
        mi *= abs(self.qi)

        # temperatures
        self.delgam_e = self.delgam
        self.delgam_i = self.temp_ratio*self.delgam_e

        if do_print:
            print("init: Positron thermal spread: ", self.delgam_i)
            print("init: Electron thermal spread: ", self.delgam_e)

        sigmaeff = self.sigma #* temperature corrections

        if do_print:
            print("init: Alfven vel:    ",sqrt(sigmaeff/(1.+sigmaeff)))
            print("init: Ion beta:      ",     2.*self.delgam_i/(self.sigma*(mi/me+1.)/(mi/me)) )
            print("init: Electron beta: ",2.*self.delgam_e/(self.sigma*(mi/me+1.)) )

        #--------------------------------------------------
        # photons
        self.qp = 0.0 # dummy charge; NOTE: needs to be 0 for photons to avoid charge deposit
        self.mp = 0.0 # dummy mass
        self.delgam_x = 0.1 #  black body temperature

        # re-name particle types
        self.prtcl_types = ['e-', 'e+'] 

        #--------------------------------------------------
        # get magnetic field from sigma: various definitions exist

        # no corrections; cold sigma
        self.binit_nc = sqrt(ppc*cfl**2.*self.sigma*me)

        #self.gammath = 1.0 + 3.0*delgam_e # approximative \gamma_th = 1 + 3\theta
        self.gammath = 1.0 + (3.0/2.0)*self.delgam_e # another approximation which is more accurate at \delta ~ 1

        #self.gammath = 1.55 #manual value for theta=0.3
        self.binit_approx = sqrt(self.gammath*ppc*me*cfl**2.*self.sigma)

        #self.binit = sqrt((self.gamma)*ppc*.5*c**2.*(me*(1.+me/mi))*self.sigma)

        self.binit = self.binit_approx # NOTE: selecting this as our sigma definitions

        if do_print:
            print("init: sigma:            ", self.sigma)
            print("init: mass term:        ", sqrt(mi+me))
            #print("init: warm corr:: ", self.warm_corr)
            print("init: gamma_th:         ", self.gammath)
            print("init: B_guide (no corr):", self.binit_nc)
            print("init: B_guide (approx): ", self.binit_approx)
            print("init: B_guide (used):   ", self.binit)


        #forcing scale in units of skin depth
        self.l0 = self.Nx*self.NxMesh/self.max_mode/self.c_omp  
        
        #thermal larmor radius
        lth = self.gammath / np.sqrt(self.sigma)*np.sqrt(self.gammath) #gamma_0

        #reconnection radius; gam ~ sigma
        lsig = self.sigma / np.sqrt(self.sigma)*np.sqrt(self.gammath) 

        # maximum attainable particle energy; from gyro-scale resonance with largest scale
        self.g0 = self.l0*np.sqrt(self.sigma)*np.sqrt(self.gammath) 


        if do_print:
            print("init: driving scale l_0:  ", self.l0)
            print("init: larmor radi r_g  :  ", lth)
            print("init: larmor rad post rec:", lsig)
            print("init: max gamma:          ", self.g0)


        #--------------------------------------------------
        # adding of static background component to particle pusher
        self.bpar = 1
        self.bperp = 1
        self.bplan = 1

        self.Lx = self.Nx*self.NxMesh
        self.Ly = self.Ny*self.NyMesh
        self.Lz = self.Nz*self.NzMesh

        # external fields
        if self.use_maxwell_split:
            self.bx_ext = self.binit*self.bpar  #np.cos(bphi)
            self.by_ext = self.binit*self.bplan #self.binit * bperp #np.sin(bphi) * np.sin(btheta)
            self.bz_ext = self.binit*self.bperp #np.sin(bphi) * np.cos(btheta)

            self.ex_ext = 0.0
            self.ey_ext = -self.beta * self.binit*self.bperp
            self.ez_ext = +self.beta * self.binit*self.bplan
        else:
            self.bx_ext = 0.0
            self.by_ext = 0.0
            self.bz_ext = 0.0

            self.ex_ext = 0.0
            self.ey_ext = 0.0
            self.ez_ext = 0.0


        #-------------------------------------------------- 
        # radiative cooling
        if "gammarad" in self.__dict__:
            if not(self.gammarad == 0.0):
                self.drag_amplitude = 0.1*self.binit/(self.gammarad**2.0)

                sigma_perp = self.sigma*self.drive_ampl**2
                self.g0 = self.l0*np.sqrt(sigma_perp)*np.sqrt(self.gammath) #gamma_0
                A = 0.1*self.g0/self.gammarad**2 #definition we use in Runko

                if do_print:
                    print("init: using radiation drag...")
                    print("init: drag amplitude: {} with gamma_rad: {}".format(self.drag_amplitude, self.gammarad))
                    print("init: rad A:        ", A)


        #-------------------------------------------------- 
        # driving
        lx = self.Nx*self.NxMesh 
        self.t0 = 1.0/(lx/self.cfl/self.max_mode) # time steps in units of light-crossing 

        if do_print:
            print("init:--------------------------------------------------")
            print("init: using Langevin antenna...")
            print("init: ampl: ", self.drive_ampl)
            print("init: w0:   ", self.drive_freq)
            print("init: g0:   ", self.decorr_time)
            print("init: t0:   ", self.t0, " laps")


        #-------------------------------------------------- 
        # default normalization

        self.e_norm = 1.0*self.binit 
        self.b_norm = 1.0*self.binit
        self.j_norm = abs(self.qe)*self.ppc*2*self.cfl**2
        self.p_norm = max(self.ppc*2, 1)

        # DONE
        if do_print:
            print("init:--------------------------------------------------")

        return

# end of class


# Prtcl velocity (and location modulation inside cell)
#
# NOTE: Cell extents are from xloc to xloc + 1, i.e., dx = 1 in all dims.
#       Therefore, typically we use position as x0 + RUnif[0,1).
#
# NOTE: Default injector changes odd ispcs's loc to (ispcs-1)'s prtcl loc.
#       This means that positrons/ions are on top of electrons to guarantee
#       charge conservation (because zero charge density initially).
#
def velocity_profile(xloc, ispcs, conf):

    # electrons
    if ispcs == 0:
        delgam = conf.delgam_e  # * np.abs(conf.mi / conf.me) * conf.temp_ratio

    # positrons/ions/second species
    elif ispcs == 1:
        delgam = conf.delgam_i

    # photons
    elif ispcs == 2:
        delgam = conf.delgam_x

    # perturb position between x0 + RUnif[0,1)
    xx = xloc[0] + np.random.rand()
    yy = xloc[1] + np.random.rand()
    zz = xloc[2] + np.random.rand()

    # velocity sampling from Maxwell-Juttner
    if ispcs in [0,1]:
        gamma = 0 # no bulk motion
        direction = +1
        ux, uy, uz, uu = pytools.sample_boosted_maxwellian(
            delgam, gamma, direction=direction, dims=3
        )
    elif ispcs == 2: # photons
        ux, uy, uz, uu = pytools.sample_blackbody(delgam)

        #if np.isnan(ux) or np.isnan(uy) or np.isnan(uz) or np.isnan(uu):
        #    sys.exit()

    x0 = [xx, yy, zz]
    u0 = [ux, uy, uz]
    return x0, u0



# number of prtcls of species 'ispcs' to be added to cell at location 'xloc'
#
# NOTE: Plasma frequency is adjusted based on conf.ppc (and prtcl charge conf.qe/qi
#       and mass conf.me/mi are computed accordingly) so modulate density in respect
#       to conf.ppc only.
#
def density_profile(xloc, ispcs, conf):
    if ispcs in [0,1]:
        return conf.ppc
    if ispcs == 2:
        return conf.xpc

# Particle weight that is added to cell at location 'xloc'
def weigth_profile(xloc, ispcs, conf):
    return 1.0


