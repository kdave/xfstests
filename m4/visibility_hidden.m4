AC_DEFUN([AC_FUNC_GCC_VISIBILITY],
  [AC_CACHE_CHECK(whether __attribute__((visibility())) is supported,
		  libc_cv_visibility_attribute,
		  [cat > conftest.c <<EOF
		   int foo __attribute__ ((visibility ("hidden"))) = 1;
		   int bar __attribute__ ((visibility ("protected"))) = 1;
EOF
		  libc_cv_visibility_attribute=no
		  if ${CC-cc} -Werror -S conftest.c -o conftest.s \
			>/dev/null 2>&1; then
		    if grep '\.hidden.*foo' conftest.s >/dev/null; then
		      if grep '\.protected.*bar' conftest.s >/dev/null; then
			libc_cv_visibility_attribute=yes
		      fi
		    fi
		  fi
		  rm -f conftest.[cs]
		  ])
   if test $libc_cv_visibility_attribute = yes; then
     AC_DEFINE(HAVE_VISIBILITY_ATTRIBUTE)
   fi
  ])
