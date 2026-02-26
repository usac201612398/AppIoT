from django.urls import path
from iotappweb import views
#from app1.views import *

app_main ="iotappweb"

urlpatterns = [
    path("iot/accion",views.enviarinstruccion,name='enviarinstruccion'),
    path("planta/",views.plantadashboard,name='plantadashboard'),
    path('api/planta/', views.planta_api, name='planta_api'),
    path('tanque/', views.tanquedashboard, name='tanquedashboard'),
    path('api/tanque/', views.tanque_api, name='tanque_api'),
    path('riegos/historial', views.historial_riegos, name='riegoshistorial'),
    path('consumo/', views.consumo_acumulado, name='consumo_acumulado'),
    path('api/histograma/', views.histograma_api, name='histograma_api')
]
