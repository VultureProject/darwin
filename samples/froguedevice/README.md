# Python example filter

## Purpose of this guide and disclaimer

This is an example filter that calls Python code from Darwin and then returns the result. You can use this filter as a template project when you want to create a new Darwin filter that uses Python code.

Below are commands that help you install and configure the Python filter if you want to test it. However, this is **NOT** a real filter: the guide below just serves as an example to show you how to setup a Python-based Darwin filter.

## 1. Filter compilation

First, you need to compile the filter. After setting the `DARWIN_PROJECT_PATH` variable to your Darwin project, run these commands below:

```bash
cd $DARWIN_PROJECT_PATH
mkdir "$DARWIN_PROJECT_PATH/build" # create a build folder to contain your darwin_python_example filter built
cd "$DARWIN_PROJECT_PATH/build"
cmake .. -DFILTER=PYTHON_EXAMPLE # generate your Makefile files
make
```

## 2. Add the filter to Darwin

Then, you have to add the filter to Darwin. Just copy it to your Darwin filter folder:

```bash
cp "$DARWIN_PROJECT_PATH/build/darwin_python_example /home/darwin/filters/" # will add your darwin_python_example filter to the other existing ones
```

## 3. Configuration

Finally, you need to change your Darwin configuration to take into account the Python-based filter. **Be careful: these commands below will override your current darwin.conf!**

```bash
cp -R "$DARWIN_PROJECT_PATH/samples/fpythonexample/example_configuration_files/* /home/darwin/conf/" # will OVERRIDE your current darwin.conf!
virtualenv -p python3.6 /home/darwin/conf/fpython_example/env # the filter NEEDS a Python environment to run
/home/darwin/conf/fpython_example/env/bin/pip install -r "$DARWIN_PROJECT_PATH/samples/fpythonexample/python_example_filter/requirements.txt" # after creating it, we install the dependencies
cp -R "$DARWIN_PROJECT_PATH/samples/fpythonexample/python_example_filter/example_filter.py" /home/darwin/filters/ # here, we just copy our Python code
```
