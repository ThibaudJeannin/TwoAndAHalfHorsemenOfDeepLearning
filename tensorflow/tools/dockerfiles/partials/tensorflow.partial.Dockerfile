ARG TF_PACKAGE
RUN ${PIP} install ${TF_PACKAGE}
RUN ${PIP} install keras
RUN ${PIP} install theano
RUN ${PIP} install pandas
RUN ${PIP} install sklearn
