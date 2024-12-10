         PROGRAM ZRGV
c **********************************************************************
C *  This program reads as input MERGE data files                      *
C *  and lists out the bad gauges			               *
C **********************************************************************
C *    PROGRAM WRITTEN BY: EYAL AMITAI                                 *
c *                        JCET/UMBC - GSFC/NASA                       *
c **********************************************************************
C *    PROGRAM WRITTEN:    Dec 9, 1998	                               *
c **********************************************************************
C *    PROGRAM WAS LAST MODIFIED:   April 29, 1999                     *
c **********************************************************************
* * Interface mods by John H. Merritt, SM&A Corp.  Dec 9, 1998
*
*   Mods include:
*
*      1. Parsing code to allow for reading unmodified second
*         intermediate (radar gauge merged) files.
*
*      2. Tracking the start and stop times for output.
*
*      3. Output only bad gauges and their time ranges.
c **********************************************************************

      integer scans, zresol
      PARAMETER (scans = 350000, zresol=800, ngs=120)
      DIMENSION RRATE(15)
      DIMENSION IRANGE(scans),ICS(scans)
      DIMENSION IRINP(scans,2),IZINP(scans),IRIN(scans)
      DIMENSION it1(9),it2(9),zdb1(9),zdb2(9)
      DIMENSION IMIN(2,2)
      DIMENSION NSCANGB(ngs),NSCANG(ngs)
      DIMENSION X15(scans),NREF(scans)
      DIMENSION IQC(scans)
      REAL      KMP,RMP(0:zresol)
      CHARACTER*80  IFILE
      CHARACTER*350 LINE
      character*3 cnet
      character*20 IDGAUGE(scans), IDGAUGEOLD, gtime(scans)
      character*20 GTIME1BAD(ngs),GTIME2BAD(ngs),IDGAUGEBAD(ngs) 
      logical qverbose
      DIMENSION IGOODSHIFT(41),IGOODSHIFTFLAG(-20:20),NNET(scans)
      character*3 CNETOLD,NAMECNET(5)
      DIMENSION RGAUGETNET(ngs),RRADARTNET(ngs)
      DIMENSION RGAUGESNET(ngs),RRADARSNET(ngs)
      DIMENSION RGAUGECNET(ngs),RRADARCNET(ngs)
      DIMENSION RGAUGESZNET(ngs),RRADARSZNET(ngs)
      DIMENSION RGAUGECZNET(ngs),RRADARCZNET(ngs)
      DIMENSION RGAUGEZNET(ngs),RRADARZNET(ngs)
      DIMENSION RGAUGE_NO_R(ngs),RRADAR_NO_G(ngs)
      DIMENSION NGNET(ngs),P(14,5),PT(14),IGOOD(ngs),PRECENT(14)
      character*11 TOTAL(2)

      do i=1,scans
         irange(i) = 0
         ics(i) = 0
         irinp(i,1) = 0
         irinp(i,2) = 0
         izinp(i) = 0
         x15(i) = 0
         nref(i) = 0
         iqc(i) = 0
         NNET(I)=0
      enddo

      do i=0,zresol
         rmp(i) = 0
      enddo

*-------------------------------------------------------------------------
* Define defaults
      iunit         = 5
      iuse_neg      = 1
      iwg           = 1
      imin_rr       = 7
      idbzthreshold = -999
      iuse_class0   = 1     ! -majority
      kmp           = 300
      alpha         = 1.4
      ifile         = ''
      r_no_g        = 0.4
      g_no_r        = 0.2
      cc_arg        = 0.15
      bias_low      = 0.5
      bias_high     = 2.0
c      qverbose      = .true.
      qverbose      = .false.
      irangemin     =15.0
      irangemax     =99.0
      majorityalways=0
      
* Default to using negative gauge values, unless ...      
      call read_runtime(iunit, ifile, qverbose,
     + iuse_neg, iwg, imin_rr, imin,
     + idbzthreshold,
     + iuse_class0,
     + iy1, imon1, id1, ih1, im1, decday1,
     + iy2, imon2, id2, ih2, im2, decday2,
     + kmp, alpha,
     + r_no_g, g_no_r, cc_arg, bias_low, bias_high)


        A=10*ALOG10(KMP)
        DO 328 I=0,zresol
           RMP(I)=10.**(0.1/ALPHA*(I/10.-A))
 328    CONTINUE
        
        
        
C     ***** READ THE MERGE DATA FILES.
        NSCAN=0
* 5=stdin, 6-stdout, 0=stderr
        if (qverbose) then
           WRITE(0,203)IFILE
 203       FORMAT(' Read merged data from ',A80)
        endif
        if (ifile .ne. '') then
           OPEN(UNIT=iunit,FILE=IFILE,STATUS='unknown',ERR=1004)
        endif
c     Skip header lines
 3      READ(iunit,201)LINE
 201    FORMAT(A80)
        IF(LINE(1:12).NE.'Table begins')GOTO3
        
        RGAUGET=0
        RGAUGEN=0
        RGAUGEZ=0
        RGAUGES=0
        RGAUGEC=0
        DO NG=1,100
          RGAUGETNET(NG)=0
          RRADARTNET(NG)=0
          RGAUGESNET(NG)=0
          RRADARSNET(NG)=0
          RGAUGECNET(NG)=0
          RRADARCNET(NG)=0
          RGAUGESZNET(NG)=0
          RRADARSZNET(NG)=0
          RGAUGECZNET(NG)=0
          RRADARCZNET(NG)=0
          RGAUGE_NO_R(NG)=0
          RRADAR_NO_G(NG)=0
          RGAUGEZNET(NG)=0
          RRADARZNET(NG)=0
        ENDDO
        NET=0
        CNETOLD='   '
        IDGAUGEOLD='   '
        NG=0    
CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
 1      call parse_input(iunit,
     +       idg,CNET,IMON,ID,IYR,IHOUR,IMINUTE,RANGE, level,
     +       xh1,iz1, it1,zdb1,
     +       xh2,iz2,it2,zdb2,
     +       inu,RRATE,*98)

CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
       

        NSCAN=NSCAN+1
        IF(NSCAN.GT.scans)STOP 'INCREASE DIMENSIONS OF PARAMETER SCANS'
        
        write(IDGAUGE(NSCAN),'(a3,i5)') cnet, idg
        write(gtime(nscan),
     +       '(i2.2,''/'',i2.2,''/'',i4,'' '',i2.2,'':'',i2.2)')
     +       imon,id,iyr,ihour,iminute

        IF(CNET.NE.CNETOLD)THEN
          NET=NET+1
          NAMECNET(NET)=CNET
          CNETOLD=CNET
        ENDIF
        NNET(NSCAN)=NET
        
        IF(IDGAUGE(NSCAN).NE.IDGAUGEOLD)THEN
          NG=NG+1
          IF(NG.GT.ngs)STOP 'INCREASE DIMENSIONS OF PARAMETER ngs'
          NGNET(NG)=NET
          IDGAUGEOLD=IDGAUGE(NSCAN)
        ENDIF



        IQC(NSCAN)=0
        IF(RANGE.LT.irangemin .OR. RANGE.GT.irangemax)IQC(NSCAN)=-1
        IF(RANGE.LT.100)THEN
          IF(zdb1(5).LE.IDBZTHRESHOLD)IQC(NSCAN)=-1
cc        IF(zdb1(5).LE.IDBZTHRESHOLD)GOTO 1
        ELSE
          IF(zdb2(5).LE.IDBZTHRESHOLD)IQC(NSCAN)=-1
cc        IF(zdb2(5).LE.IDBZTHRESHOLD)GOTO 1
        ENDIF
        
        DO I=IMIN(1,1),IMIN(IWG,2)
          IF(RRATE(I).LT.-500)IQC(NSCAN)=-1
cc         IF(RRATE(I).LT.-500)GOTO 1
          IF(RRATE(I).LT.0)THEN
            IF(IUSE_NEG.EQ.1)THEN
              RRATE(I)=ABS(RRATE(I))
            ELSE
              IQC(NSCAN)=-1
cc            GOTO 1
            ENDIF
          ENDIF
        ENDDO
        
        DO J=1,IWG
           RR=0
           DO I=IMIN(J,1),IMIN(J,2)
              RR=RR+RRATE(I)
           ENDDO
           RR=RR/(IMIN(J,2)-IMIN(J,1)+1)
           IRINP(NSCAN,J)=RR*100+0.5
        ENDDO
        
        
c     Putting the relevant data (classification parameters) into arrays
        IRANGE(NSCAN)=RANGE+0.5
        
        R15=0
        DO I=1,15
           R15=R15+RRATE(I)
        ENDDO
        X15(NSCAN)=R15/15
        
C     Count radar reflectivities
        
        NSTRAT=0
        NCONV=0
        it=1
        IF(RANGE.LT.100)THEN
           IF(zdb1(5).LE.0.0)zdb1(5)=1.0
           IZINP(NSCAN)=zdb1(5)*10.+0.5
           ICS(NSCAN)=it1(5)
           N=0
           DO I=1,9
              IF(zdb1(I).GE.20)N=N+1
           ENDDO
           NREF(NSCAN)=N
           IF(it1(5).EQ.0 .AND. IUSE_CLASS0.EQ.1 .OR. majorityalways.EQ.
     +     1)THEN
              DO I=1,9
                 IF(it1(I).EQ.1)NSTRAT=NSTRAT+1
                 IF(it1(I).EQ.2)NCONV=NCONV+1
              ENDDO
              it=0
           ENDIF
        ELSE
           IF(zdb2(5).LE.0.0)zdb2(5)=1.0
           IZINP(NSCAN)=zdb2(5)*10.+0.5
           ICS(NSCAN)=it2(5)
           N=0
           DO I=1,9
              IF(zdb2(I).GE.20)N=N+1
           ENDDO
           NREF(NSCAN)=N
           IF(it2(5).EQ.0 .AND. IUSE_CLASS0.EQ.1 .OR. majorityalways.EQ.
     +     1)THEN
              DO I=1,9
                 IF(it2(I).EQ.1)NSTRAT=NSTRAT+1
                 IF(it2(I).EQ.2)NCONV=NCONV+1
              ENDDO
              it=0
           ENDIF
        ENDIF
        IRECLASS=0
        IF(it.EQ.0)THEN
           IF(NSTRAT.GT.NCONV)THEN
             ICS(NSCAN)=1
             IRECLASS=1
           ENDIF
           IF(NSTRAT.LT.NCONV)THEN
             ICS(NSCAN)=2
             IRECLASS=1
           ENDIF
        ENDIF
        
        RZ=0
        IF(IZINP(NSCAN).GE.100)RZ=RMP(IZINP(NSCAN))

        
        IF(IQC(NSCAN).NE.-1)THEN
           RGAUGET=RGAUGET+RR   
           IF(ICS(NSCAN).EQ.-1)RGAUGEN=RGAUGEN+RR
           IF(ICS(NSCAN).EQ.0)RGAUGEZ=RGAUGEZ+RR
           IF(ICS(NSCAN).EQ.1)RGAUGES=RGAUGES+RR
           IF(ICS(NSCAN).EQ.2)RGAUGEC=RGAUGEC+RR

           RGAUGETNET(NG)=RGAUGETNET(NG)+RR       !1
           RRADARTNET(NG)=RRADARTNET(NG)+RZ       !2

           IF(IRECLASS.EQ.0)THEN
             IF(ICS(NSCAN).EQ.1)THEN
               RGAUGESNET(NG)=RGAUGESNET(NG)+RR   !3
               RRADARSNET(NG)=RRADARSNET(NG)+RZ   !4
             ENDIF
             IF(ICS(NSCAN).EQ.2)THEN
               RGAUGECNET(NG)=RGAUGECNET(NG)+RR   !5
               RRADARCNET(NG)=RRADARCNET(NG)+RZ   !6
             ENDIF
           ENDIF
           IF(IRECLASS.EQ.1)THEN
             IF(ICS(NSCAN).EQ.1)THEN
               RGAUGESZNET(NG)=RGAUGESZNET(NG)+RR !7
               RRADARSZNET(NG)=RRADARSZNET(NG)+RZ !8
             ENDIF
             IF(ICS(NSCAN).EQ.2)THEN
               RGAUGECZNET(NG)=RGAUGECZNET(NG)+RR !9
               RRADARCZNET(NG)=RRADARCZNET(NG)+RZ !10
             ENDIF
           ENDIF
           IF(ICS(NSCAN).EQ.0)THEN
             RGAUGEZNET(NG)=RGAUGEZNET(NG)+RR     !13
             RRADARZNET(NG)=RRADARZNET(NG)+RZ     !14
           ENDIF
         ENDIF
        
        
        GOTO 1                  ! READ NEXT SCAN

        
 98     CLOSE(iunit)
        
        if (qverbose) then
          WRITE(0,*)'RAIN RATES ACCUMULATION BY RAIN TYPE FROM ALL GAUGE
     +S WITHIN THE SELECTED RANGE (ALL,-,0,S,C):',RGAUGET,RGAUGEN,
     +    RGAUGEZ,RGAUGES,RGAUGEC
        endif
        
c     ******************************
        NG=1
        NSCANGB(NG)=1
        IDGAUGEOLD=IDGAUGE(1)
        DO N=2,NSCAN
           IF(IDGAUGE(N).NE.IDGAUGEOLD)THEN
              NG=NG+1
              NSCANGB(NG)=N
              NSCANG(NG-1)=N-NSCANGB(NG-1)
              IDGAUGEOLD=IDGAUGE(N)
           ENDIF
        ENDDO
        NSCANG(NG)=NSCAN-NSCANGB(NG)+1

cccccccccccccccccccccccccccccccccccccccccccccccccc
C The following was added on 4/20/99
        IF(qverbose)THEN
          WRITE(6,*)
          WRITE(6,*)
          WRITE(6,*)'PERFORMANCE OF ALL GAUGES WITHIN THE SELECTED RANGE
     + INTERVAL W/O & UPON TIME SHIFTS'          
          WRITE(6,*)
          WRITE(6,*)'Gauge ID        No shift            Best shift     
     +           good shifts'
          WRITE(6,*)'            good(1)    Corr.  #Scans   Corr.    #Pa
     +irs          Scan  #s'
        ENDIF
        DO NGAUGE=1,NG
          N1=NSCANGB(NGAUGE)
          N2=NSCANGB(NGAUGE)+NSCANG(NGAUGE)-1
c         correlation upon time shifts     !added on 4/22
          BESTCC=-999                       
          CCOLD=-999                                
          NBESTSHIFT=-999
          NCORBESTCC=-999
          NGOODSHIFT=0
          DO NSHIFT=-20,20                 
            RAD_NO_G=0
            G_NO_RAD=0
            NRAD0=0
            XRAD0=0
            Y2=0
            N2G0=0
            Y2G0=0
            NCOR=0
            XMEAN=0
            YMEAN=0
            DO N=N1,N2
              IF(N+NSHIFT.GE.N1 .AND. N+NSHIFT.LE.N2 .AND.      
     +        ICS(N).GE.0 .AND. IQC(N).EQ.0 .AND.
     +        ICS(N+NSHIFT).GE.0 .AND. IQC(N+NSHIFT).EQ.0)THEN  
                X=FLOAT(IRINP(N+NSHIFT,1))/100.0 
                Y=0
                IF(IZINP(N).GE.100)Y=RMP(IZINP(N))
                NCOR=NCOR+1
                XMEAN=XMEAN+X
                YMEAN=YMEAN+Y

                IF(Y.EQ.0 .AND. X.GT.0)THEN  !Rad<10dBZ, Gauge>0
                  NRAD0=NRAD0+1   !# OF PAIRS
C                 XRAD0 - GAUGE RAIN AMOUNT WHEN RADAR<10dBZ
                  XRAD0=XRAD0+X
                ENDIF
                IF(Y.GT.2 .AND. NREF(N).EQ.9)THEN  
                  Y2=Y2+Y       !Rad>2mm/h & all 9 pixels³20dBZ 
                  IF(X15(N+NSHIFT).EQ.0)THEN   !Gauge15min=0
c                   Y2G0 - RADAR RAIN AMOUNT (FROM R³2mm/h) WHILE GAUGE=0
                    N2G0=N2G0+1     !# OF PAIRS
                    Y2G0=Y2G0+Y
                  ENDIF
                ENDIF

              ENDIF
            ENDDO

            IF(NSHIFT.EQ.0)THEN 
              RGAUGE_NO_R(NGAUGE)=RGAUGE_NO_R(NGAUGE)+XRAD0     !11
              RRADAR_NO_G(NGAUGE)=RRADAR_NO_G(NGAUGE)+Y2G0      !12
            ENDIF


            IF(Y2.GT.0)RAD_NO_G=Y2G0/Y2
            IF(XMEAN.GT.0)G_NO_RAD=XRAD0/XMEAN

            IF(NCOR.GT.0)XMEAN=XMEAN/NCOR
            IF(NCOR.GT.0)YMEAN=YMEAN/NCOR
            X2=0
            SUMXY=0
            SUMX2=0
            SUMY2=0
            CC=0
            DO N=N1,N2
              IF(N+NSHIFT.GE.N1 .AND. N+NSHIFT.LE.N2 .AND.      
     +        ICS(N).GE.0 .AND. IQC(N).EQ.0 .AND.
     +        ICS(N+NSHIFT).GE.0 .AND. IQC(N+NSHIFT).EQ.0)THEN 
                X=FLOAT(IRINP(N+NSHIFT,1))/100.0
                Y=0
                IF(IZINP(N).GE.100)Y=RMP(IZINP(N))
                X2=X2+X**2
                SUMXY=SUMXY+(X-XMEAN)*(Y-YMEAN)
                SUMX2=SUMX2+(X-XMEAN)**2
                SUMY2=SUMY2+(Y-YMEAN)**2
              ENDIF
            ENDDO
            IF(SUMX2*SUMY2.GT.0)CC=SUMXY/(SUMX2*SUMY2)**0.5



            IBAD=0
            IF(NCOR.EQ.0 .OR. RAD_NO_G.GE.r_no_g .OR.
     +      G_NO_RAD.GE.g_no_r .OR.
     +      CC.LE.cc_arg .OR. XMEAN.EQ.0)IBAD=1
            IF(NSHIFT.EQ.0)THEN 
              CC0=CC
              IF(IBAD.EQ.0)THEN      ! NSHIFT=0 is a good gauge
                TOTALX=TOTALX+XMEAN
                TOTALY=TOTALY+YMEAN
              ENDIF
            ENDIF
            
            IGOODSHIFTFLAG(NSHIFT)=0     ! initialized as bad gauge
            IF(IBAD.EQ.0)THEN   !  if good gauge...
              IGOODSHIFTFLAG(NSHIFT)=1
              NGOODSHIFT=NGOODSHIFT+1
              IGOODSHIFT(NGOODSHIFT)=NSHIFT
              IF(CC.GT.CCOLD)THEN
                BESTCC=CC
                NBESTSHIFT=NSHIFT
                CCOLD=CC
                NCORBESTCC=NCOR
              ENDIF
            ENDIF
c            if(IDGAUGE(N1).eq.'SFL  195            ')then
c              if(NSHIFT.eq.-20)OPEN(UNIT=2,FILE='gauge195.dat',STATUS=
c     +           'unknown')
c              write(2,202)IBAD,NSHIFT,NCOR,RAD_NO_G,N2G0,
c     +        G_NO_RAD,NRAD0,CC,XMEAN*NCOR
c 202          FORMAT(3I8,F9.2,I7,F9.2,I7,F9.2,F7.0)
c            endif
          ENDDO
          
          IF(qverbose .AND. IRANGE(N1).GE.irangemin 
     +      .AND. IRANGE(N1).LE.irangemax)THEN
            WRITE(6,605)IDGAUGE(N1),IGOODSHIFTFLAG(0),CC0,NBESTSHIFT,
     +      BESTCC,NCORBESTCC,(IGOODSHIFT(I),I=1,NGOODSHIFT) 
 605        FORMAT(A10,I8,F10.2,I8,F10.2,I8,10X,41I4)
          ENDIF
        ENDDO
        
        IF(TOTALY.GT.0)TOTALBIAS=TOTALX/TOTALY
CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC


        IF(qverbose)THEN
          WRITE(6,*)
          WRITE(6,*)' Gauge_ID          Range  #pairs   Rad>2mm/h,G15=0
     + Rad<10dBZ,G>0     r     G     R/G'
          WRITE(6,*)'                                    & Rad9>=20dBZ'
          WRITE(6,*)'                                    V frac  #pairs
     +V frac  #pairs'
        ENDIF
        NGOOD=0
        NBAD=0
        NBADTOTAL=0
        DO NGAUGE=1,NG
          N1=NSCANGB(NGAUGE)
          N2=NSCANGB(NGAUGE)+NSCANG(NGAUGE)-1
          NCOR=0
          XMEAN=0
          YMEAN=0
          RAD_NO_G=0
          G_NO_RAD=0
          NRAD0=0
          XRAD0=0
          Y2=0
          N2G0=0
          Y2G0=0
          DO N=N1,N2
            IF(ICS(N).GE.0 .AND. IQC(N).EQ.0)THEN
cc          IF(ICS(N).GE.0)THEN
              X=FLOAT(IRINP(N,1))/100.0
              Y=0
              IF(IZINP(N).GE.100)Y=RMP(IZINP(N))
              NCOR=NCOR+1
              XMEAN=XMEAN+X
              YMEAN=YMEAN+Y
              IF(Y.EQ.0 .AND. X.GT.0)THEN !Rad<10dBZ, Gauge>0
                NRAD0=NRAD0+1 !# OF PAIRS
C               XRAD0 - GAUGE RAIN AMOUNT WHEN RADAR<10dBZ
                XRAD0=XRAD0+X
              ENDIF
              IF(Y.GT.2 .AND. NREF(N).EQ.9)THEN  
                Y2=Y2+Y     !Rad>2mm/h & all 9 pixels>=20dBZ 
                IF(X15(N).EQ.0)THEN !Gauge15min=0
c                 Y2G0 - RADAR RAIN AMOUNT (FROM R³2mm/h) WHILE GAUGE=0
                  N2G0=N2G0+1 !# OF PAIRS
                  Y2G0=Y2G0+Y
                ENDIF
              ENDIF
            ENDIF
          ENDDO
          IF(Y2.GT.0)RAD_NO_G=Y2G0/Y2
          IF(XMEAN.GT.0)G_NO_RAD=XRAD0/XMEAN
          IF(NCOR.GT.0)XMEAN=XMEAN/NCOR
          IF(NCOR.GT.0)YMEAN=YMEAN/NCOR
          DO N=N1,N2
            IRIN(N)=IRINP(N,1)
          ENDDO
          CALL CORR(ICS,IQC,IRIN,IZINP,XMEAN,YMEAN,N1,N2,RMP,CC)
          IBAD=0
          IF(NCOR.EQ.0 .OR. RAD_NO_G.GE.r_no_g .OR.
     +    G_NO_RAD.GE.g_no_r .OR.
     +    CC.LE.cc_arg .OR. XMEAN.EQ.0)IBAD=1
          ROVERG=-99
          IF(XMEAN.GT.0)THEN
            ROVERG=TOTALBIAS*YMEAN/XMEAN
            IF(ROVERG.LE.bias_low .OR. ROVERG.GE.bias_high)IBAD=1
          ENDIF
          IF(IBAD.EQ.1)THEN
            IF(qverbose .AND. IRANGE(N1).GE.irangemin 
     +      .AND. IRANGE(N1).LE.irangemax)THEN
              WRITE(6,603)IDGAUGE(N1),IRANGE(N1),NCOR,RAD_NO_G,N2G0,
     +             G_NO_RAD,NRAD0,CC,XMEAN*NCOR,ROVERG
 603          FORMAT('B ',a15,I7,I8,F9.2,I7,F9.2,I7,F9.2,F7.0,F7.2)
              NBAD=NBAD+1
            ENDIF
            NBADTOTAL=NBADTOTAL+1
            GTIME1BAD(NBADTOTAL)=gtime(n1)
            GTIME2BAD(NBADTOTAL)=gtime(n2)
            IDGAUGEBAD(NBADTOTAL)=IDGAUGE(N1)
            IGOOD(NGAUGE)=0
          ELSE
            NGOOD=NGOOD+1
            IGOOD(NGAUGE)=1
          ENDIF
        ENDDO
        IF(qverbose)THEN
          
          DO NTABLE=1,2
 
            DO L=1,14
              DO N=1,NGNET(NG)
                P(L,N)=0
                PT(L)=0
              ENDDO
            ENDDO
            DO I=1,NG
              IF(NTABLE.EQ.1 .OR. NTABLE.EQ.2 .AND. IGOOD(I).EQ.1)THEN
                N=NGNET(I)
                P(1,N)=P(1,N)+RGAUGETNET(I)    
                P(2,N)=P(2,N)+RRADARTNET(I)    
                P(3,N)=P(3,N)+RGAUGESNET(I)    
                P(4,N)=P(4,N)+RRADARSNET(I)    
                P(5,N)=P(5,N)+RGAUGECNET(I)    
                P(6,N)=P(6,N)+RRADARCNET(I)    
                P(7,N)=P(7,N)+RGAUGESZNET(I)    
                P(8,N)=P(8,N)+RRADARSZNET(I)    
                P(9,N)=P(9,N)+RGAUGECZNET(I)    
                P(10,N)=P(10,N)+RRADARCZNET(I)    
                P(11,N)=P(11,N)+RGAUGE_NO_R(I)    
                P(12,N)=P(12,N)+RRADAR_NO_G(I)    
                P(13,N)=P(13,N)+RGAUGEZNET(I)    
                P(14,N)=P(14,N)+RRADARZNET(I)
              ENDIF  
            ENDDO
            DO L=1,14
              DO N=1,NGNET(NG)
                PT(L)=PT(L)+P(L,N)
              ENDDO
            ENDDO

            DO L=3,14
              I=1
              A=FLOAT(L)/2.
              J=FLOAT(L)/2.
              IF(A.EQ.J)I=2              
              IF(PT(I).EQ.0)THEN
                PRECENT(L)=-999
              ELSE
                PRECENT(L)=100*PT(L)/PT(I)
              ENDIF
            ENDDO
          
            WRITE(6,*)
            WRITE(6,*)
            IF(NTABLE.EQ.1)WRITE(6,*)'ALL GAUGES WITHIN SELECTED RANGE'
            IF(NTABLE.EQ.2)WRITE(6,*)'ALL QC GAUGES (W/O timing correcti
     +ons)'
            WRITE(6,*)
            TOTAL(1)='      TOTAL'
            TOTAL(2)=' */TOTAL[%]'
            WRITE(6,607)(NAMECNET(N),N=1,NNET(NSCAN)),TOTAL(1),TOTAL(2)
         
            WRITE(6,608)(P(1,N),N=1,NNET(NSCAN)),PT(1)
            WRITE(6,609)(P(2,N),N=1,NNET(NSCAN)),PT(2)
            WRITE(6,610)(P(3,N),N=1,NNET(NSCAN)),PT(3),PRECENT(3)
            WRITE(6,611)(P(4,N),N=1,NNET(NSCAN)),PT(4),PRECENT(4)
            WRITE(6,612)(P(5,N),N=1,NNET(NSCAN)),PT(5),PRECENT(5)
            WRITE(6,613)(P(6,N),N=1,NNET(NSCAN)),PT(6),PRECENT(6)
            WRITE(6,614)(P(7,N),N=1,NNET(NSCAN)),PT(7),PRECENT(7)
            WRITE(6,615)(P(8,N),N=1,NNET(NSCAN)),PT(8),PRECENT(8)
            WRITE(6,616)(P(9,N),N=1,NNET(NSCAN)),PT(9),PRECENT(9)
            WRITE(6,617)(P(10,N),N=1,NNET(NSCAN)),PT(10),PRECENT(10)
            WRITE(6,618)(P(11,N),N=1,NNET(NSCAN)),PT(11),PRECENT(11)
            WRITE(6,619)(P(12,N),N=1,NNET(NSCAN)),PT(12),PRECENT(12)
            WRITE(6,620)(P(13,N),N=1,NNET(NSCAN)),PT(13),PRECENT(13)
            WRITE(6,621)(P(14,N),N=1,NNET(NSCAN)),PT(14),PRECENT(14)
 607        FORMAT('            ',5(A11))
 608        FORMAT('G TOTAL:    ',5F11.1)
 609        FORMAT('R TOTAL:    ',5F11.1)
 610        FORMAT('G STRA:     ',5F11.1)
 611        FORMAT('R STRA:     ',5F11.1)
 612        FORMAT('G CONV:     ',5F11.1)
 613        FORMAT('R CONV:     ',5F11.1)
 614        FORMAT('G STRA RT0: ',5F11.1)
 615        FORMAT('R STRA RT0: ',5F11.1)
 616        FORMAT('G CONV RT0: ',5F11.1)
 617        FORMAT('R CONV RT0: ',5F11.1)
 618        FORMAT('G NO R:     ',5F11.1)
 619        FORMAT('R NO G:     ',5F11.1)
 620        FORMAT('G RT0:      ',5F11.1)
 621        FORMAT('R RT0:      ',5F11.1)

            IF(PT(1).EQ.0)THEN
              ROVERGTOTAL=-9.99
              KMPTOTAL=-999
            ELSE
              ROVERGTOTAL=PT(2)/PT(1)
              KMPTOTAL=kmp*ROVERGTOTAL**alpha+0.5
            ENDIF
            write(6,*)
            write(6,622)ROVERGTOTAL,KMPTOTAL,ALPHA
 622        FORMAT('TOTAL R/G,A,b= ',F5.2,I5,F5.2)

            IF((PT(3)+PT(7)).EQ.0)THEN
              ROVERGSTRA=-9.99
              KMPSTRA=-999
            ELSE
              ROVERGSTRA=(PT(4)+PT(8))/(PT(3)+PT(7))
              KMPSTRA=kmp*ROVERGSTRA**alpha+0.5
            ENDIF
            write(6,623)ROVERGSTRA,KMPSTRA,ALPHA
 623        FORMAT('STRA  R/G,A,b= ',F5.2,I5,F5.2)

            IF((PT(5)+PT(9)).EQ.0)THEN
              ROVERGCONV=-9.99
              KMPCONV=-999
            ELSE
              ROVERGCONV=(PT(6)+PT(10))/(PT(5)+PT(9))
              KMPCONV=kmp*ROVERGCONV**alpha+0.5
            ENDIF
            write(6,624)ROVERGCONV,KMPCONV,ALPHA
 624        FORMAT('CONV  R/G,A,b= ',F5.2,I5,F5.2)
          ENDDO

          WRITE(6,*)
          WRITE(6,606)NGOOD,NBAD
 606      FORMAT('# OF GOOD & BAD GAUGES WITHIN THE AQC RANGE INTERVAL P
     +REVIOUS TO ANY TIMING CORRECTIONS,'/
     +' RESPECTIVELY: ',2I5)
          WRITE(6,*)
          WRITE(6,*)'# OF ALL GAUGES (BAD AND GOOD ALL RANGES): ',NG
          WRITE(6,*)
        ENDIF
        DO N=1,NBADTOTAL
          WRITE(6,604)GTIME1BAD(N),GTIME2BAD(N),IDGAUGEBAD(N)
 604      FORMAT(3a20)
        ENDDO
        call exit
 1004   STOP 'CANT OPEN INPUT DATA FILE'
        END
      


        SUBROUTINE CORR(IC,IQ,IX,IY,XM,YM,NSTART,NEND,RPL,C)
        integer scans, zresol
        PARAMETER (scans = 150000, zresol=800)
        DIMENSION IC(scans),IQ(scans)
        DIMENSION IX(scans),IY(scans)
        REAL      RPL(0:zresol)
        X2=0
        SUMXY=0
        SUMX2=0
        SUMY2=0
        C=0
        DO N=NSTART,NEND
          IF(IC(N).GE.0 .AND. IQ(N).EQ.0)THEN
            X=FLOAT(IX(N))/100.0
            Y=0
            IF(IY(N).GE.100)Y=RPL(IY(N))
            X2=X2+X**2
            SUMXY=SUMXY+(X-XM)*(Y-YM)
            SUMX2=SUMX2+(X-XM)**2
            SUMY2=SUMY2+(Y-YM)**2
          ENDIF
        ENDDO
        IF(SUMX2*SUMY2.GT.0)C=SUMXY/(SUMX2*SUMY2)**0.5
        RETURN
        END


*********************************************************************
**                                                                 **
**                       usage                                     **
**                                                                 **
*********************************************************************

      subroutine usage

      write(0,*)'     Usage: eyalqc [-p] [-iwg n] [-m n] [-dbzth' //
     +'resh x]'
      write(0,*)'               [-v]'
      write(0,*)'               [-center|-majority]'
      write(0,*)'               [-A x] [-b x]'
      write(0,*)'               [-r_no_g x]'
      write(0,*)'               [-g_no_r x]'
      write(0,*)'               [-cc     x]'
      write(0,*)'               [-bias_low  x]'
      write(0,*)'               [-bias_high x]'
      write(0,*)'               [second_intermediate_file]'
      write(0,*)''
      write(0,*)'    -v     Verbose.'
      write(0,*)'    -p     Use positive values only.  Default: Us' //
     +'e both negative and positive gauge rainrates.'
      write(0,*)'    -iwg n ''n'' is # R values/scan.  Default: 1'
      write(0,*)'    -m n   ''n'' is # minutes for'
      write(0,*)'           gauge rr avg.            Default: 7'
      write(0,*)'    -dbzthresh x '
      write(0,*)'           ''x'' is max dBz to not'
      write(0,*)'            use.                    Default: -9' //
     +'99 (use all)'
      write(0,*)'    -center|-majority               Default: -m' //
     +'ajority'
      write(0,*)'    -A x                            Default: 30' //
     +'0.00'
      write(0,*)'    -b x                            Default: 1.4'
      write(0,*)''
      write(0,*)' This is the Bad gauge criteria.  Any of:'
      write(0,*)'    -r_no_g x      (>=x)            Default: 0.4'
      write(0,*)'    -g_no_r x      (>=x)            Default: 0.2'
      write(0,*)'    -cc     x      (<=x)            Default: 0.15'
      write(0,*)'    -bias_low  x   (<=x)            Default: 0.5'
      write(0,*)'    -bias_high x   (>=x)            Default: 2.0'

      call exit
      end
*********************************************************************
**                                                                 **
**                    read_runtime                                 **
**                                                                 **
*********************************************************************

      subroutine read_runtime(iunit, ifile, qverbose,
     + iuse_neg, iwg, imin_rr, imin,
     + idbzthreshold,
     + iuse_class0,
     + iy1, imon1, id1, ih1, im1, decday1,
     + iy2, imon2, id2, ih2, im2, decday2,
     + kmp, alpha,
     + r_no_g, g_no_r, cc_arg, bias_low, bias_high)

      real kmp
      integer imin(2,2)
      character*(*) ifile
      character*50 carg
      logical qverbose

      nargs = iargc()
* while (i <= nargs)
      i = 1
      if (i .le. nargs) then
         call getarg(i, carg)
         if (carg .eq. "-p") then
            iuse_neg = 0
         else if (carg .eq. "-v") then
            qverbose = .true.
         else if (carg .eq. "-h" .or. carg .eq. "-help") then
            call usage()
         else if (carg .eq. "-iwg") then
            i = i+1
            call getarg(i, carg)
            if (carg .eq. '') call usage()
            read(carg,*) iwg
         else if (carg .eq. "-m") then
            i = i+1
            call getarg(i, carg)
            if (carg .eq. '') call usage()
            read(carg,*) imin_rr
         else if (carg .eq. "-r_no_g") then
            i = i+1
            call getarg(i, carg)
            if (carg .eq. '') call usage()
            read(carg,*) r_no_g
         else if (carg .eq. "-r_no_g") then
            i = i+1
            call getarg(i, carg)
            if (carg .eq. '') call usage()
            read(carg,*) r_no_g
         else if (carg .eq. "-g_no_r") then
            i = i+1
            call getarg(i, carg)
            if (carg .eq. '') call usage()
            read(carg,*) g_no_r
         else if (carg .eq. "-cc") then
            i = i+1
            call getarg(i, carg)
            if (carg .eq. '') call usage()
            read(carg,*) cc_arg
         else if (carg .eq. "-bias_low") then
            i = i+1
            call getarg(i, carg)
            if (carg .eq. '') call usage()
            read(carg,*) bias_low
         else if (carg .eq. "-bias_high") then
            i = i+1
            call getarg(i, carg)
            if (carg .eq. '') call usage()
            read(carg,*) bias_high
         else if (carg .eq. "-dbzthresh") then
            i = i+1
            call getarg(i, carg)
            if (carg .eq. '') call usage()
            read(carg,*) idbzthreshold
         else if (carg .eq. "-center") then
            iuse_class0 = 0
         else if (carg .eq. "-majority") then
            iuse_class0 = 1
         else if (carg .eq. "-A") then
            i = i+1
            call getarg(i, carg)
            if (carg .eq. '') call usage()
            read(carg,*) kmp
         else if (carg .eq. "-b") then
            i = i+1
            call getarg(i, carg)
            if (carg .eq. '') call usage()
            read(carg,*) alpha
         else
            ifile = carg
            open(iunit, file=ifile, status='old',iostat=ios)
            if (ios .ne. 0) then
               write(0,*) 'Error: unable to open file:',carg
               call exit
            endif
         endif
      endif


      IF(IUSE_NEG.EQ.1 .and. qverbose)THEN
         WRITE(0,*)'Converting negative gauge rain rate to positive va
     +lues.'
      ENDIF
      IF(IUSE_NEG.EQ.0 .and. qverbose)THEN
         WRITE(0,*)'Not using negative gauge rain rate values.'
      ENDIF
      
      IF(IWG.NE.1 .AND. IWG.NE.2)STOP 'Illegal Gauge window size'
      IF(IWG*IMIN_RR.GT.15)STOP 'Illegal Gauge time window input value'
      IF(IWG.EQ.1)THEN
         if (qverbose) then
            WRITE(0,601)IMIN_RR
         endif
 601     FORMAT('# of minutes for averaging a rainrate value centered
     + on the radar scan time:' ,I3)
         IMIN(IWG,1)=8-IMIN_RR/2
         IMIN(IWG,2)=8+IMIN_RR/2
         if (qverbose) then
            WRITE(0,*)'IMIN1,IMIN2: ',IMIN(IWG,1),' - ',IMIN(IWG,2)
         endif
      ELSE
         if (qverbose) then
            WRITE(0,611)IMIN_RR
         endif
 611     FORMAT('# of minutes for averaging a rainrate value before & after
     +          the radar scan time:' ,I3)
         IMIN(1,1)=8-IMIN_RR+1
         IMIN(1,2)=8
         IMIN(2,1)=9
         IMIN(2,2)=8+IMIN_RR
         if (qverbose) then
            WRITE(0,*)'IMIN1,IMIN2: ',IMIN(1,1),' - ',IMIN(1,2)
            WRITE(0,*)'IMIN1,IMIN2: ',IMIN(2,1),' - ',IMIN(2,2)
         endif
      ENDIF
        
        
      if (qverbose) then
         WRITE(0,602)IDBZTHRESHOLD
      endif
 602  FORMAT('Maximum reflectivity value [dBZ] not to be used by PMM:
     +       ' ,I5)
        
      IF(IUSE_CLASS0.EQ.1 .and. qverbose)THEN
         WRITE(0,*)'Replacing raintype=0 according to window majority'
      ENDIF
        
      return
      end


*********************************************************************
**                                                                 **
**                    parse_input                                  **
**                                                                 **
*********************************************************************
        subroutine parse_input(iunit,
     +       idg,CNET,IMON,ID,IYR,IHOUR,IMINUTE,RANGE, level,
     +       xh1,iz1, it1,zdb1,
     +       xh2,iz2,it2,zdb2,
     +       inu,RRATE,*)

        real rrate(15)
        integer it1(9), it2(9)
        real    zdb1(9), zdb2(9)
        character*(*) cnet

        character*500 line
        character*20 token, get_token

        read(iunit,'(A)',end=500) line

* The order is: gaugeid, network, date, time, data
* 'data' will be read via the normal fortran read routine.
*

* Grab the first 4 strings.
        ipos = 0
        token = get_token(line(1:), ip)
        ipos = ipos + ip
        read(token,*) idg

        cnet = get_token(line(ipos:), ip)
        ipos = ipos + ip

        token = get_token(line(ipos:), ip)
        ipos = ipos + ip
* Parse the date.
        read(token,'(i2,1x,i2,1x,i4)') imon, id, iyr

        token = get_token(line(ipos:), ip)
        ipos = ipos + ip
* Parse the time.
        read(token,'(i2,1x,i2)') ihour, iminute

        read(line(ipos:), *) RANGE, level,
     +       xh1,iz1, (it1(i),zdb1(i), i=1,iz1),
     +       xh2,iz2, (it2(i),zdb2(i), i=1,iz2),
     +       inu,(RRATE(i),i=1,inu)

*        write(30,1001) RANGE, level,
*     +       xh1,iz1, (it1(i),zdb1(i), i=1,iz1),
*     +       xh2,iz2, (it2(i),zdb2(i), i=1,iz2),
*     +       inu,(RRATE(i),i=1,inu)
* 1001   format(2f8.2,2(f8.2,i8,9(i8,f8.2)),i8,15f8.2)
        return
 500    return 1
        end

      character*20 function get_token(string, ipos)
      character*(*) string
      integer ipos

* Skip all leading ' '.

      get_token = ' '
      iend = len(string)
      do i=1,iend
         if (string(i:i) .ne. ' ') go to 10
      enddo

 10   if (i .ge. iend) then
         ipos = i
         return
      endif

* From here to the next space.

      do j=i,iend
         if (string(j:j) .eq. ' ') then
            ipos = j
            get_token = string(i:j-1)
            return
         endif
      enddo

      return
      end
