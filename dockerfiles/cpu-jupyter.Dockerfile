# Copyright 2018 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================
#
# THIS IS A GENERATED DOCKERFILE.
#
# This file was assembled from multiple pieces, whose use is documented
# below. Please refer to the the TensorFlow dockerfiles documentation for
# more information. Build args are documented as their default value.
#
# Ubuntu-based, CPU-only environment for using TensorFlow, with Jupyter included.
#
# Start from Ubuntu (no GPU support)
# --build-arg UBUNTU_VERSION=16.04
#    ( no description )
#
# Python is required for TensorFlow and other libraries.
# --build-arg USE_PYTHON_3_NOT_2=True
#    Install python 3 over Python 2
#
# Install the TensorFlow Python package.
# --build-arg TF_PACKAGE=tensorflow (tensorflow|tensorflow-gpu|tf-nightly|tf-nightly-gpu)
#    The specific TensorFlow Python package to install
#
# Configure TensorFlow's shell prompt and login tools.
#
# Launch Jupyter on execution instead of a bash prompt.

ARG UBUNTU_VERSION=16.04
FROM ubuntu:${UBUNTU_VERSION}

ARG USE_PYTHON_3_NOT_2=True
ARG _PY_SUFFIX=${USE_PYTHON_3_NOT_2:+3}
ARG PYTHON=python${_PY_SUFFIX}
ARG PIP=pip${_PY_SUFFIX}

RUN apt-get update && apt-get install -y \
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

ARG TF_PACKAGE=tensorflow
RUN ${PIP} install ${TF_PACKAGE}
RUN ${PIP} install keras
RUN ${PIP} install theano
RUN ${PIP} install pandas
RUN ${PIP} install sklearn

COPY bashrc /etc/bash.bashrc
RUN chmod a+rwx /etc/bash.bashrc

RUN ${PIP} install jupyter

RUN mkdir /notebooks && chmod a+rwx /notebooks
RUN mkdir /.local && chmod a+rwx /.local
RUN R -e "IRkernel::installspec()"
RUN ${PIP} install jupyterthemes
RUN jt -t monokai -cellw 90% -T
WORKDIR /notebooks
EXPOSE 8888

CMD ["bash", "-c", "source /etc/bash.bashrc && jupyter notebook --notebook-dir=/notebooks --ip 0.0.0.0 --no-browser --allow-root"]
