CXXFLAGS = -Wall -std=c++0x
LDFLAGS = -lz

SRCS =                   \
    d_dehtbl.cpp         \
    d_io.cpp             \
    d_wads.cpp           \
    e_hash.cpp           \
    e_rtti.cpp           \
    i_system.cpp         \
    main.cpp             \
    m_argv.cpp           \
    m_buffer.cpp         \
    metaapi.cpp          \
    metaqstring.cpp      \
    m_fcvt.cpp           \
    m_misc.cpp           \
    m_qstr.cpp           \
    m_strcasestr.cpp     \
    psnprntf.cpp         \
    s_sfxgen.cpp         \
    tables.cpp           \
    v_psx.cpp            \
    w_formats.cpp        \
    w_wad.cpp            \
    w_zip.cpp            \
    zip_write.cpp        \
    z_native.cpp

OBJS=$(patsubst %.cpp,%.o,$(SRCS))

psxwadgen : $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) -o $@

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

