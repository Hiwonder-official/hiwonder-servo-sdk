from setuptools import setup, find_packages

setup(
    name="hiwonder-servo",
    version="1.0.0",
    description="Python SDK for HX-30HM bus servo",
    packages=find_packages(where="."),
    package_dir={"": "."},
    install_requires=["pyserial>=3.5"],
    python_requires=">=3.8",
)
