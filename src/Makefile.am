bin_PROGRAMS = ztbackup
AM_CPPFLAGS = ${LIBSSH2_CFLAGS} ${GLIB_CFLAGS} ${LIBSSH_CFLAGS} ${LIBSMB_CFLAGS}
ztbackup_SOURCES = main.cpp BackupFolder.cpp LocalBackupFolder.cpp LogFile.cpp SmbBackupFolder.cpp Ssh2BackupFolder.cpp ExcludeList.cpp Util.cpp
ztbackup_LDADD = ${LIBSSH2_LIBS} ${GLIB_LIBS} ${LIBSSH_LIBS} ${LIBSMB_LIBS}
