# CPack pre-build script: gives the staged tree the modes Debian policy wants.
# If the build tree has default ACLs, the staged files carry raw group/other ACL entries and
# CPack's tar step packs those instead of the chmod'ed mode (-rw-rwxr-- files, drwxrwxrwx dirs).
# So: drop the ACLs (no setfacl / no ACLs: nothing to drop), then set 644 / 755 explicitly,
# because removing the ACL leaves the raw group entry as the group bits.
find_program( GLE_SETFACL setfacl )
if( GLE_SETFACL )
   execute_process( COMMAND "${GLE_SETFACL}" -R -b "${CPACK_TEMPORARY_DIRECTORY}"
                    ERROR_QUIET OUTPUT_QUIET )
endif()

file( CHMOD_RECURSE "${CPACK_TEMPORARY_DIRECTORY}"
      FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
      DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                            GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE )
file( CHMOD "${CPACK_TEMPORARY_DIRECTORY}/usr/bin/GameListEditor"
      PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE )
