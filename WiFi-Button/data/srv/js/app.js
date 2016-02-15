var app;

app = angular.module("ApiWebClient", ["ngRoute", "ngResource", "ngMaterial"]);

app.run([
  "$rootScope", "$route", function($rootScope, $route) {
    return $rootScope.$on("$routeChangeSuccess", function(currRoute, prevRoute) {
      if (!!$route.current.pageTitle) {
        return $rootScope.pageTitle = $route.current.pageTitle;
      } else {
        return $rootScope.pageTitle = null;
      }
    });
  }
]);

app.config([
  "$httpProvider", function($httpProvider) {
    $httpProvider.defaults.useXDomain = true;
    return delete $httpProvider.defaults.headers.common["X-Requested-With"];
  }
]);

app.config([
  "$mdThemingProvider", function($mdThemingProvider) {
    return $mdThemingProvider.theme("default").primaryPalette("light-blue").accentPalette("deep-orange");
  }
]);

app.config([
  "$mdIconProvider", function($mdIconProvider) {
    return $mdIconProvider.defaultFontSet("material-icons");
  }
]);

app.config([
  "$routeProvider", "$locationProvider", function($routeProvider, $locationProvider) {
    return $routeProvider.when("/", {
      templateUrl: "welcome.html",
      controller: "WelcomeController",
      pageTitle: "Welcome"
    }).when("/sample", {
      templateUrl: "sample.html",
      controller: "SampleController",
      pageTitle: "Sample API Call"
    }).when("/settings", {
      templateUrl: "settings.html",
      controller: "SettingsController",
      pageTitle: "Settings"
    }).otherwise({
      redirectTo: "/"
    });
  }
]);

var app;

app = angular.module("ApiWebClient");

app.controller("AppController", [
  "$rootScope", "$scope", "$mdSidenav", "$mdDialog", function($rootScope, $scope, $mdSidenav, $mdDialog) {
    $scope.toggleSidenav = function() {
      $mdSidenav("left").toggle();
    };
    $scope.closeSidenav = function() {
      $mdSidenav("left").close();
    };
    $rootScope.showAlertDialog = function(ev, title, content, ariaLabel, ok) {
      if (ariaLabel == null) {
        ariaLabel = null;
      }
      if (ok == null) {
        ok = "OK";
      }
      if (ariaLabel == null) {
        ariaLabel = content;
      }
      return $mdDialog.show($mdDialog.alert().title(title).content(content).ariaLabel(ariaLabel).ok(ok).targetEvent(ev));
    };
  }
]);

app.controller("SidenavController", [
  "$scope", "$route", "$mdSidenav", function($scope, $route, $mdSidenav) {
    $scope.items = [
      {
        link: "#/sample",
        title: "Sample"
      }, {
        link: "#/settings",
        title: "Settings"
      }
    ];
    $scope.isActive = function(path) {
      if ($route.current && $route.current.regexp) {
        return $route.current.regexp.test(path);
      }
      return false;
    };
    $scope.close = function() {
      $mdSidenav("left").close();
    };
  }
]);

app.controller("WelcomeController", ["$scope", function($scope) {}]);

app.controller("SampleController", [
  "$rootScope", "$scope", "$http", "Relay", "Button", function($rootScope, $scope, $http, Relay, Button) {
    $scope.toggleRelay = function(ev) {
      var state;
      state = $scope.relay ? 'ON' : 'OFF';
      Relay.get({
        state: state
      }).$promise.then(function(data) {
        var text;
        if (data.status === "OK") {
          return $scope.relay;
        } else {
          text = "There was an error. The server's response is: " + (JSON.stringify(data));
          return $rootScope.showAlertDialog(ev, "Relay Error", text, null, "Got It!");
        }
      });
    };
    $scope.getButtonState = function(ev) {
      Button.get().$promise.then(function(data) {
        var text;
        text = "The server's response is: " + (JSON.stringify(data));
        return $rootScope.showAlertDialog(ev, "Button State", text, null, "Got It!");
      });
    };
    $scope.uploadFirmware = function(ev) {
      var f, r;
      f = document.getElementById('firmware').files[0];
      r = new FileReader();
      r.readAsBinaryString(f);
      r.onloadend = function() {
        return $http.post('/api/upload/firmware', r.result, {
          transformRequest: angular.identity,
          headers: {
            'Content-Type': void 0
          }
        });
      };
    };
    $scope.setUploadPath = function(ev) {
      $http.get("/api/upload/path?path=" + $scope.uploadPath);
    };
    return $scope.uploadFile = function(ev) {
      var f, r;
      f = document.getElementById('file').files[0];
      r = new FileReader();
      r.readAsBinaryString(f);
      r.onloadend = function() {
        return $http.post('/api/upload/file', r.result, {
          transformRequest: angular.identity,
          headers: {
            'Content-Type': void 0
          }
        });
      };
    };
  }
]);

app.controller("SettingsController", ["$scope", function($scope) {}]);

var app;

app = angular.module("ApiWebClient");

app.directive("awcSideNav", function() {
  return {
    restrict: "A",
    templateUrl: "awc-side-nav.html",
    controller: "SidenavController"
  };
});

var app;

app = angular.module("ApiWebClient");

app.factory("Relay", function($resource) {
  var Relay;
  return Relay = $resource("/api/gpio/relay");
});

app.factory("Button", function($resource) {
  var Button;
  return Button = $resource("/api/gpio/buttonState");
});
