#!/bin/sh
# takes in $1 the name of the output library and in
# $2... names of input libraries

AR=ar
AR_EXTRACT_FLAGS=x
AR_CREATE_FLAGS=r

RANLIB=ranlib

TMP_DIR=`mktemp -d`

LIBNAME=$1
echo "NAME $LIBNAME"
shift


for i in "$@"
do
  echo "SRC $i"
  cp ${i} ${TMP_DIR}
done

echo "LIBS copied"
(
	cd ${TMP_DIR}
	for i in *.a
	do
		${AR} ${AR_EXTRACT_FLAGS} ${i}
	done 
	rm *.a
	${AR} ${AR_CREATE_FLAGS} ${LIBNAME} *.o
	rm *.o
)

cp ${TMP_DIR}/${LIBNAME} .
rm -rf ${TMP_DIR}
${RANLIB} ${LIBNAME}
