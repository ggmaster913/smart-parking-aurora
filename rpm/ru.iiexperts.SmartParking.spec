# Сторонний QML-плагин MfwMap (/usr/share/<пакет>/lib) не экспортирует и не требует
# системных зависимостей (как в официальном MapTemplate).
%global __provides_exclude_from ^%{_datadir}/%{name}/.*$
%global __requires_exclude_from ^%{_datadir}/%{name}/.*$

Name:       ru.iiexperts.SmartParking
Summary:    Умная парковка: поиск и оплата парковки (демо)
Version:    0.1
Release:    3
License:    MIT
URL:        https://auroraos.ru
Source0:    %{name}-%{version}.tar.bz2

Requires:   sailfishsilica-qt5 >= 0.10.9
Requires:   qt5-qtdeclarative-import-positioning
BuildRequires:  pkgconfig(auroraapp)
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Sql)
BuildRequires:  pkgconfig(Qt5Positioning)

%description
Прототип команды ИИксперты: поиск парковки на карте, выбор места, симуляция оплаты и электронный талон. Данные парковок вымышлены.

%prep
%autosetup

%build
%qmake5
%make_build

%install
%make_install

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%defattr(644,root,root,-)
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
