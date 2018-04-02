Name: point-set-viewer
Version:	0.1.0
Release:	1%{?dist}
Summary:	Point set explorer

License:	GPLv3
URL:		https://github.com/santileortiz/point-set-viewer

Source0:  https://github.com/santileortiz/%{name}/archive/%{version}.tar.gz

BuildRequires: python3
BuildRequires: gcc
BuildRequires: cairo-devel
BuildRequires: pango-devel

Requires:	pango
Requires:	libX11-xcb

%description
A tool to explore different point sets in two dimensions and their combinatoric
properties.

%prep
%setup -n %{name}-%{version} 

%build
./pymk.py point_set_viewer --mode release

%install
./pymk.py install --destdir %{buildroot}/

%files
/usr/bin/*
/usr/share/applications/*
/usr/share/icons/hicolor/*/apps/*

%changelog

