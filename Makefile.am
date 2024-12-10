AUTOMAKE_OPTIONS = foreign

INCLUDES = -I$(prefix)/include -I$(prefix)/toolkit/include -DPROG_VERSION=\"$(PACKAGE)-$(VERSION)\" -I/usr/local/include

bin_PROGRAMS = \
 build_dual_zr \
 build_gauge_db \
 build_pmm_zr_table \
 build_single_zr \
 build_zr_histo \
 build_zr_table \
 eyalqc \
 first2ascii \
 gauge_gui.pl \
 get_2A53_data_over_gauge \
 get_radar_data_over_gauge \
 listdb \
 merge_radarNgauge_data  \
 merge_zr_histo \
 query_gauge_db \
 scale_zr_table \
 validate_gauge_db

build_dual_zr_SOURCES             = build_dual_zr.c zr.c zr.h zr_table.h
build_gauge_db_SOURCES            = build_gauge_db.c gauge_db.c gauge_db.h
build_pmm_zr_table_SOURCES        = build_pmm_zr_table.c 2A53.h zr_utils.c zr_utils.h zr.c zr.h
build_single_zr_SOURCES           = build_single_zr.c zr.c zr.h zr_table.h
build_zr_histo_SOURCES            = build_zr_histo.c zr_utils.c zr_utils.h zr.c zr.h
build_zr_table_SOURCES            = build_zr_table.c zr.c zr.h zr_table.h getopt.c getopt1.c getopt.h
eyalqc_SOURCES                    = eyalqc.f
first2ascii_SOURCES               = first2ascii.c get_radar_data_over_gauge_db.h zr.h  output.c gauge_db.c gauge_db.h
gauge_gui_pl_SOURCES              = 
gauge_gui_pl_DEPENDENCIES         = eyalqc
get_2A53_data_over_gauge_SOURCES  = get_2A53_data_over_gauge.c utils.c output.c gauge_db.c gauge_db.h get_2A53_data_over_gauge.h
get_radar_data_over_gauge_SOURCES = get_radar_data_over_gauge.c get_radar_data_over_gauge.h zr.h gauge_db.h 2A53.h output.c utils.c gauge_db.c
listdb_SOURCES                    = listdb.c
merge_radarNgauge_data_SOURCES    = merge_radarNgauge_data.c gauge_db.h utils.c gauge_db.c gauge_db.h
merge_zr_histo_SOURCES            = merge_zr_histo.c zr_utils.c zr_utils.h zr.c zr.h
query_gauge_db_SOURCES            = query_gauge_db.c gauge_db.c gauge_db.h
scale_zr_table_SOURCES            = scale_zr_table.c zr.c zr.h  zr_table.h
validate_gauge_db_SOURCES         = validate_gauge_db.c gauge_db.c gauge_db.h

utils.o: zr.h Makefile
gauge_db.o: gauge_db.h Makefile
output.o: get_radar_data_over_gauge.h zr.h get_radar_data_over_gauge_db.h gauge_db.h

bin_SCRIPTS = $(regular_scripts) $(xforms_scripts)
regular_scripts = \
          accum_plot.cmd \
          automatic_qc.pl \
          automatic_zr.pl \
          merge_2nd_zr_intermediate_files \
          gauge_filter.pl \
          gauge_gui_main.pl \
          gauge_qc.pl \
          gauge_radar_accum.pl \
          gauge_radar_nonzero.pl \
          gauge_radar_plot_batch.pl \
          gauge_radar_plot_filter.pl \
          gauge_radar_plot_subtract.pl \
          make_gauge_filter_file.pl \
          make_when_table \
          process_first_zr_inter_product_for_non_hdf_files \
          process_first_zr_inter_product_for_tape \
          process_first_zr_inter_product_for_tapes \
          r_over_g.pl \
          sort_zr_intermediate_ascii \
          sort_zr_intermediate_ascii_by_time \
          synthesize_gauge.pl

xforms_scripts = gauge_gui.pl

gauge_gui.pl$(EXEEXT): gauge_gui.fd Makefile
	-fdesign -convert -perl gauge_gui.fd > /dev/null 2>&1

scripts = $(bin_SCRIPTS)

EXTRA_DIST = gauge_gui.fd $(doc_DATA) $(scripts) CHANGES CHECKLIST \
             Copyright GPL
CLEANFILES = gauge_gui.pl 

docdir = $(prefix)/doc/build_zr_table
doc_DATA = \
      automatic_qc.html \
      build_gauge_db.html \
      build_zr_table.html \
      automatic_zr.html \
      build_pmm_zr_table.html \
      gauge_filter.html \
      gauge_gui.html \
      gauge_gui_screenshot.html \
      gauge_qc.html \
      gauge_radar_accum.html \
      get_2A53_data_over_gauge.html \
      get_radar_data_over_gauge.html \
      merge_radarNgauge_data.html \
      r_over_g.html \
      sort_zr_intermediate_ascii.html \
      zr.html \
      zr_content.html \
      zr_diagram.gif \
      to_zr_logo.gif \
      zr_logo.gif \
      gauge_gui_main.gif \
      gauge_gui_data.gif \
      gauge_gui_gauges.gif \
	  README CHANGES


automatic_qc.pl: Makefile
	@for p in $(regular_scripts); do \
	   echo -n "Checking file $$p ... ";\
	   cp $$p $$p.in;\
	   sed -e 's&PROG_VERSION[ ]*=.*&PROG_VERSION = \"$(PACKAGE)-$(VERSION)\";&' \
		-e 's&#\!.*perl&#\! $(perlbin)&' \
		-e 's&(\@INC,.*&(\@INC, \".\", \"$(bindir)\"\)\;&' \
	   < $$p.in > $$p.new;\
	   if cmp -s $$p $$p.new; then \
          rm $$p.new; \
          echo "$$p remains untouched."; \
       else \
          mv $$p.new $$p; \
          echo "substitutions made in $$p."; \
		  chmod +x $$p; \
       fi;\
	   rm -f $$p.in;\
    done


