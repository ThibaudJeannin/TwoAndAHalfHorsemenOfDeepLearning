RUN ${PIP} install jupyter

RUN mkdir /notebooks && chmod a+rwx /notebooks
RUN mkdir /.local && chmod a+rwx /.local
RUN R -e "IRkernel::installspec()"
RUN ${PIP} install jupyterthemes
RUN ${PIP} install jupyter_contrib_nbextensions jupyter_nbextensions_configurator
RUN jupyter contrib nbextension install --user
RUN jt -t monokai -cellw 90% -T
WORKDIR /notebooks
EXPOSE 8888

CMD ["bash", "-c", "source /etc/bash.bashrc && jupyter notebook --notebook-dir=/notebooks --ip 0.0.0.0 --no-browser --allow-root"]
