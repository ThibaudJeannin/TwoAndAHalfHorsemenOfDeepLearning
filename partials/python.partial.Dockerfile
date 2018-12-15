ARG USE_PYTHON_3_NOT_2
ARG _PY_SUFFIX=${USE_PYTHON_3_NOT_2:+3}
ARG PYTHON=python${_PY_SUFFIX}
ARG PIP=pip${_PY_SUFFIX}

RUN apt-get update && apt-get install -y \
    git \
    ${PYTHON} \
    ${PYTHON}-pip \
    r-base \
	  r-base-dev \
    libssl-dev \
	  libcurl4-openssl-dev \
	  libxml2-dev

RUN ${PIP} install --upgrade \
    pip \
    setuptools

RUN R -e "install.packages(c('keras', 'devtools', 'caret', 'rpart', 'tidyverse', 'optparse', 'testthat', 'corrplot', 'repr', 'IRdisplay', 'evaluate', 'crayon', 'pbdZMQ', 'devtools', 'uuid', 'digest'), quiet = FALSE, repos = c('https://pbil.univ-lyon1.fr/CRAN/','https://mirror.ibcp.fr/pub/CRAN/'))"
#RUN R -e "keras::install_keras()"
RUN R -e "devtools::install_github('IRkernel/IRkernel')"
