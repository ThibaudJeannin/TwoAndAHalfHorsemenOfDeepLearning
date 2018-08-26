RUN ${PIP} install jupyter

RUN mkdir /notebooks && chmod a+rwx /notebooks
RUN mkdir /.local && chmod a+rwx /.local
RUN R -e "IRkernel::installspec()"
RUN ${PIP} install jupyterthemes
RUN jt -t onedork jt -cellw 90%
WORKDIR /notebooks
EXPOSE 8888

CMD ["bash", "-c", "source /etc/bash.bashrc && jupyter notebook --notebook-dir=/notebooks --ip 0.0.0.0 --no-browser --allow-root"]
