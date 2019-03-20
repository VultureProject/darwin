# D.A.R.W.I.N

Darwin is an open source **Artificial Intelligence Framework for CyberSecurity**. It can be compiled and run on **both FreeBSD and Linux**.

**Darwin will be available on this repository in the near future.**

We provide packages and support for FreeBSD.

Darwin is:
 - A multi-threaded C++ engine that runs **security filters** that work together to improve your network security
 - A collection of **agents** that use the DARWIN protocol to query the security filters accordingly

Darwin is still in an alpha stage, so few filters are available at this time:
 - A **reputation** filter, that check the reputation on Ip Address, based on libmaxminddb (https://dev.maxmind.com/maxmind-db/)
 - A **web injection** filter, based on a [extreme gradient booster](https://xgboost.readthedocs.io/en/latest/)

Using the provided documentation and SDK you can develop your own Darwin Filters.

At this time we provide the following agents with VultureOS:
 - An **HAProxy agent**: turn HAProxy into a WAF!
 - An **Rsyslog agent (mmdarwin)**: detect security incidents directly from your logs!

We are seeking help! Testers and volunteers are welcome!


Advens (www.advens.fr) also provides commercial filters for Darwin:
 - A **domain generation algorithms (DGA) detector** filter, which uses [TensorFlow](https://www.tensorflow.org/)
 - A **bad user agent detector** filter, which uses [TensorFlow](https://www.tensorflow.org/)
 - Many others to come...