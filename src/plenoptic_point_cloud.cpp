#include "plenoptic_point_cloud.h"

vector<PPC*> make_incremental_Plen_PC(
	vector<Mat> color_imgs,
	vector<Mat> depth_imgs,
	int colorspace,
	vector<int> camera_order)
{
	/////////////////////////
	int c_threshold = 7;
	int d_threshold;
	/////////////////////////

	vector<PPC*> Plen_PC;
	PointCloud<PointXYZRGB>::Ptr firstPC;

	firstPC = make_PC(0, color_imgs[0], depth_imgs[0]);

	// 첫번째 view 
	for (int point_idx = 0; point_idx < firstPC->points.size(); point_idx++) {
		PPC* point_ppc;

		if (version == 1.0) point_ppc = new PPC_v1();
		else if (version == 2.1) point_ppc = new PPC_v2_1();
		else if (version == 2.2) point_ppc = new PPC_v2_2();

		point_ppc->SetGeometry(firstPC->points[point_idx]);

		if (version == 2.1 || version == 3.1) point_ppc->SetRefColor(firstPC->points[point_idx], 0);
		else if (version == 1.0 || version == 2.2 || version == 3.2) point_ppc->SetColor(firstPC->points[point_idx], 0);

		Plen_PC.push_back(point_ppc);
	}

	vector<float> min(3), max(3);
	find_min_max(Plen_PC, min, max);

	cout << "range of Z : " << max[2] - min[2] << endl;

	//d_threshold = (max[2] - min[2]) / 100;

	for (int i = 1; i < total_num_cameras; i++) {
		int cam = i;// camera_order[i];

		int u;
		int v;

		double w;

		Mat color_img(_height, _width, CV_8UC3, Scalar(0));
		Mat confirm_img(_height, _width, CV_32S, -1);

		Vec3b d;
		Vec3s d_s;

		for (int point_idx = 0; point_idx < Plen_PC.size(); point_idx++) {
			float* geometry = Plen_PC[point_idx]->GetGeometry();

			double X = geometry[0];
			double Y = geometry[1];
			double Z = geometry[2];

			w = projection_XYZ_2_UV(
				m_CalibParams[cam].m_ProjMatrix,
				X,
				Y,
				Z,
				u,
				v);

			if ((u < 0) || (v < 0) || (u > _width - 1) || (v > _height - 1)) continue;

			confirm_img.at<int>(v, u) = point_idx;
		}

		double max_dist = 0.;

		for (int v = 0; v < _height; v++) {
			for (int u = 0; u < _width; u++) {
				Vec3b color = color_imgs[cam].at<Vec3b>(v, u);

				double X;
				double Y;
				double Z;

				switch (data_mode) {
				case 0:
					d = depth_imgs[cam].at<Vec3b>(v, u);
					Z = depth_level_2_Z(d[0]);
					break;
				case 1:
				case 2:
					d_s = depth_imgs[cam].at<Vec3s>(v, u);
					Z = depth_level_2_Z_s(d_s[0]);
					break;
				case 3:
					d_s = depth_imgs[cam].at<Vec3s>(v, u);
					Z = depth_level_2_Z_s(d_s[0], cam);
					break;
				case 4:
				case 5:
				case 6:
				case 7:
				case 8:
				case 9:
				case 10:
				case 11:
				case 12:
				case 13:
					Z = depth_level_2_Z_s_direct(depth_imgs[cam].at<ushort>(v, u));
					break;
				}

				double Z_origin = Z;
				if (!data_mode) projection_UVZ_2_XY_PC(m_CalibParams[cam].m_ProjMatrix, u, v, Z, &X, &Y);
				else Z = MVG(m_CalibParams[cam].m_K, m_CalibParams[cam].m_RotMatrix, m_CalibParams[cam].m_Trans, u, v, Z, &X, &Y);

				//if ((data_mode == 8 || data_mode == 9) && depth_imgs[cam].at<ushort>(v, u) >= 9000) continue;

				// projection not ok
				if (confirm_img.at<int>(v, u) == -1) {

					PPC* point_ppc;
					if (version == 1.0) point_ppc = new PPC_v1();
					else if (version == 2.1) point_ppc = new PPC_v2_1();
					else if (version == 2.2) point_ppc = new PPC_v2_2();

					float geo[3] = { X,Y,Z };
					point_ppc->SetGeometry(geo);
					if (version == 2.1) point_ppc->SetRefColor(color, cam);
					else if (version == 1.0 || version == 2.2) point_ppc->SetColor(color, cam);

					Plen_PC.push_back(point_ppc);
				}
				// projection ok
				else
				{
					int point_idx = confirm_img.at<int>(v, u);

					int nearCamNum = 0;
					for (int c = cam - 1; c >= 0; c--) {
						if (!Plen_PC[point_idx]->CheckOcclusion(c)) {
							nearCamNum = c;
							break;
						}
					}

					double w_origin, w_point;

					w_origin = m_CalibParams[cam].m_ProjMatrix(2, 0) * X + m_CalibParams[cam].m_ProjMatrix(2, 1) * Y +
						m_CalibParams[cam].m_ProjMatrix(2, 2) * Z + m_CalibParams[cam].m_ProjMatrix(2, 3);

					float* geo = Plen_PC[point_idx]->GetGeometry();

					w_point = m_CalibParams[cam].m_ProjMatrix(2, 0) * geo[0] + m_CalibParams[cam].m_ProjMatrix(2, 1) * geo[1] +
						m_CalibParams[cam].m_ProjMatrix(2, 2) * geo[2] + m_CalibParams[cam].m_ProjMatrix(2, 3);

					double sub_dist = find_point_dist(w_point, cam) - find_point_dist(w_origin, cam);

					if (max_dist < sub_dist) max_dist = sub_dist;

					/*if (abs(sub_dist) > d_threshold) {
						PPC* point_ppc;
						if (version == 1.0) point_ppc = new PPC_v1();
						else if (version == 2.1) point_ppc = new PPC_v2_1();
						else if (version == 2.2) point_ppc = new PPC_v2_2();

						float geo[3] = { X,Y,Z };
						point_ppc->SetGeometry(geo);
						if (version == 2.1) point_ppc->SetRefColor(color, cam);
						else if (version == 1.0 || version == 2.2) point_ppc->SetColor(color, cam);

						Plen_PC.push_back(point_ppc);
						continue;
					}*/

					if (colorspace == 0) { //YUV
						int sub_V = color[2] - Plen_PC[point_idx]->GetColor(nearCamNum)[0];
						int sub_U = color[1] - Plen_PC[point_idx]->GetColor(nearCamNum)[1];

						// color thres
						if (abs(sub_U) < c_threshold && abs(sub_V) < c_threshold) {
							Plen_PC[point_idx]->SetColor(color, cam);
						}
						else {
							PPC* point_ppc;
							if (version == 1.0) point_ppc = new PPC_v1();
							else if (version == 2.1) point_ppc = new PPC_v2_1();
							else if (version == 2.2) point_ppc = new PPC_v2_2();

							float geo[3] = { X,Y,Z };
							point_ppc->SetGeometry(geo);
							if (version == 2.1) point_ppc->SetRefColor(color, cam);
							else if (version == 1.0 || version == 2.2) point_ppc->SetColor(color, cam);

							Plen_PC.push_back(point_ppc);
						}
					}
					else if (colorspace == 1) { //HSV

						Mat hsv_org(1, 1, CV_8UC3);
						hsv_org.at<Vec3b>(0, 0) = color;
						cvtColor(hsv_org, hsv_org, CV_YUV2BGR);
						cvtColor(hsv_org, hsv_org, CV_BGR2HSV);

						Mat hsv_ppc(1, 1, CV_8UC3);
						hsv_ppc.at<Vec3b>(0, 0)[0] = Plen_PC[point_idx]->GetColor(nearCamNum)[2];
						hsv_ppc.at<Vec3b>(0, 0)[1] = Plen_PC[point_idx]->GetColor(nearCamNum)[1];
						hsv_ppc.at<Vec3b>(0, 0)[2] = Plen_PC[point_idx]->GetColor(nearCamNum)[0];
						cvtColor(hsv_ppc, hsv_ppc, CV_YUV2BGR);
						cvtColor(hsv_ppc, hsv_ppc, CV_BGR2HSV);

						int sub_H = hsv_org.at<Vec3b>(0, 0)[0] - hsv_ppc.at<Vec3b>(0, 0)[0];
						int sub_S = hsv_org.at<Vec3b>(0, 0)[1] - hsv_ppc.at<Vec3b>(0, 0)[1];

						// color thres
						if (abs(sub_H) < c_threshold && abs(sub_S) < c_threshold) {
							Plen_PC[point_idx]->SetColor(color, cam);
						}
						else {
							PPC* point_ppc;
							if (version == 1.0) point_ppc = new PPC_v1();
							else if (version == 2.1) point_ppc = new PPC_v2_1();
							else if (version == 2.2) point_ppc = new PPC_v2_2();

							float geo[3] = { X,Y,Z };
							point_ppc->SetGeometry(geo);
							if (version == 2.1) point_ppc->SetRefColor(color, cam);
							else if (version == 1.0 || version == 2.2) point_ppc->SetColor(color, cam);

							Plen_PC.push_back(point_ppc);
						}
					}
					else if (colorspace == 2) { //RGB

						Mat bgr_org(1, 1, CV_8UC3);
						bgr_org.at<Vec3b>(0, 0) = color;
						cvtColor(bgr_org, bgr_org, CV_YUV2BGR);

						Mat bgr_ppc(1, 1, CV_8UC3);
						bgr_ppc.at<Vec3b>(0, 0)[0] = Plen_PC[point_idx]->GetColor(nearCamNum)[2];
						bgr_ppc.at<Vec3b>(0, 0)[1] = Plen_PC[point_idx]->GetColor(nearCamNum)[1];
						bgr_ppc.at<Vec3b>(0, 0)[2] = Plen_PC[point_idx]->GetColor(nearCamNum)[0];
						cvtColor(bgr_ppc, bgr_ppc, CV_YUV2BGR);

						int sub_B = bgr_org.at<Vec3b>(0, 0)[0] - bgr_ppc.at<Vec3b>(0, 0)[0];
						int sub_G = bgr_org.at<Vec3b>(0, 0)[1] - bgr_ppc.at<Vec3b>(0, 0)[1];
						int sub_R = bgr_org.at<Vec3b>(0, 0)[2] - bgr_ppc.at<Vec3b>(0, 0)[2];

						// color thres
						if (abs(sub_B) < c_threshold && abs(sub_G) < c_threshold && abs(sub_R) < c_threshold) {
							Plen_PC[point_idx]->SetColor(color, cam);
						}
						else {
							PPC* point_ppc;
							if (version == 1.0) point_ppc = new PPC_v1();
							else if (version == 2.1) point_ppc = new PPC_v2_1();
							else if (version == 2.2) point_ppc = new PPC_v2_2();

							float geo[3] = { X,Y,Z };
							point_ppc->SetGeometry(geo);
							if (version == 2.1) point_ppc->SetRefColor(color, cam);
							else if (version == 1.0 || version == 2.2) point_ppc->SetColor(color, cam);

							Plen_PC.push_back(point_ppc);
						}
					}
				}
			}
		}

		//cout << "max_dist : " << max_dist << endl;
	}

	Plen_PC.shrink_to_fit();

	return Plen_PC;
}

vector<PPC*> make_incremental_Plen_PC(
	vector<Mat> color_imgs,
	vector<Mat> depth_imgs,
	int colorspace,
	vector<int> camera_order,
	int voxel_div_num,
	float &depth_threshold)
{
	/////////////////////////
	int c_threshold = 7;
	/////////////////////////

	vector<PPC*> Plen_PC;
	PointCloud<PointXYZRGB>::Ptr firstPC;

	firstPC = make_PC(0, color_imgs[0], depth_imgs[0]);

	// 첫번째 view 
	for (int point_idx = 0; point_idx < firstPC->points.size(); point_idx++) {
		PPC* point_ppc;

		if (version == 1.0) point_ppc = new PPC_v1();
		else if (version == 2.1) point_ppc = new PPC_v2_1();
		else if (version == 2.2) point_ppc = new PPC_v2_2();

		point_ppc->SetGeometry(firstPC->points[point_idx]);

		if (version == 2.1) point_ppc->SetRefColor(firstPC->points[point_idx], 0);
		else if (version == 1.0 || version == 2.2) point_ppc->SetColor(firstPC->points[point_idx], 0);

		Plen_PC.push_back(point_ppc);
	}

	vector<float> min(3), max(3);
	find_min_max(Plen_PC, min, max);

	cout << "range of Z : " << max[2] - min[2] << endl;

	float Cube_z_size = max[2] - min[2];
	float cube_z_size = Cube_z_size / voxel_div_num;
	//////////////////////////////////////////
	depth_threshold = cube_z_size/10000;
	//////////////////////////////////////////
	cout << "depth_threshold : " << depth_threshold << endl;

	for (int i = 1; i < total_num_cameras; i++) {
		int cam = i;// camera_order[i];

		int u;
		int v;

		double w;

		Mat color_img(_height, _width, CV_8UC3, Scalar(0));
		Mat confirm_img(_height, _width, CV_32S, -1);

		Vec3b d;
		Vec3s d_s;

		for (int point_idx = 0; point_idx < Plen_PC.size(); point_idx++) {
			float* geometry = Plen_PC[point_idx]->GetGeometry();

			double X = geometry[0];
			double Y = geometry[1];
			double Z = geometry[2];

			w = projection_XYZ_2_UV(
				m_CalibParams[cam].m_ProjMatrix,
				X,
				Y,
				Z,
				u,
				v);

			if ((u < 0) || (v < 0) || (u > _width - 1) || (v > _height - 1)) continue;

			confirm_img.at<int>(v, u) = point_idx;
		}

		double max_dist = 0.;

		for (int v = 0; v < _height; v++) {
			for (int u = 0; u < _width; u++) {
				Vec3b color = color_imgs[cam].at<Vec3b>(v, u);

				double X;
				double Y;
				double Z;

				switch (data_mode) {
				case 0:
					d = depth_imgs[cam].at<Vec3b>(v, u);
					Z = depth_level_2_Z(d[0]);
					break;
				case 1:
				case 2:
					d_s = depth_imgs[cam].at<Vec3s>(v, u);
					Z = depth_level_2_Z_s(d_s[0]);
					break;
				case 3:
					d_s = depth_imgs[cam].at<Vec3s>(v, u);
					Z = depth_level_2_Z_s(d_s[0], cam);
					break;
				case 4:
				case 5:
				case 6:
				case 7:
				case 8:
				case 9:
				case 10:
				case 11:
				case 12:
				case 13:
					Z = depth_level_2_Z_s_direct(depth_imgs[cam].at<ushort>(v, u));
					break;
				}

				double Z_origin = Z;
				if (!data_mode) projection_UVZ_2_XY_PC(m_CalibParams[cam].m_ProjMatrix, u, v, Z, &X, &Y);
				else Z = MVG(m_CalibParams[cam].m_K, m_CalibParams[cam].m_RotMatrix, m_CalibParams[cam].m_Trans, u, v, Z, &X, &Y);

				//if ((data_mode == 8 || data_mode == 9) && depth_imgs[cam].at<ushort>(v, u) >= 9000) continue;

				// projection not ok
				if (confirm_img.at<int>(v, u) == -1) {

					PPC* point_ppc;
					if (version == 1.0) point_ppc = new PPC_v1();
					else if (version == 2.1) point_ppc = new PPC_v2_1();
					else if (version == 2.2) point_ppc = new PPC_v2_2();

					float geo[3] = { X,Y,Z };
					point_ppc->SetGeometry(geo);
					if (version == 2.1) point_ppc->SetRefColor(color, cam);
					else if (version == 1.0 || version == 2.2) point_ppc->SetColor(color, cam);

					Plen_PC.push_back(point_ppc);
				}
				// projection ok
				else
				{
					int point_idx = confirm_img.at<int>(v, u);

					int nearCamNum = 0;
					for (int c = cam - 1; c >= 0; c--) {
						if (!Plen_PC[point_idx]->CheckOcclusion(c)) {
							nearCamNum = c;
							break;
						}
					}

					double w_origin, w_point;

					w_origin = m_CalibParams[cam].m_ProjMatrix(2, 0) * X + m_CalibParams[cam].m_ProjMatrix(2, 1) * Y +
						m_CalibParams[cam].m_ProjMatrix(2, 2) * Z + m_CalibParams[cam].m_ProjMatrix(2, 3);

					float* geo = Plen_PC[point_idx]->GetGeometry();

					w_point = m_CalibParams[cam].m_ProjMatrix(2, 0) * geo[0] + m_CalibParams[cam].m_ProjMatrix(2, 1) * geo[1] +
						m_CalibParams[cam].m_ProjMatrix(2, 2) * geo[2] + m_CalibParams[cam].m_ProjMatrix(2, 3);

					double sub_dist = find_point_dist(w_point, cam) - find_point_dist(w_origin, cam);

					if (max_dist < sub_dist) max_dist = sub_dist;

					if (abs(sub_dist) > depth_threshold) {
						PPC* point_ppc;
						if (version == 1.0) point_ppc = new PPC_v1();
						else if (version == 2.1) point_ppc = new PPC_v2_1();
						else if (version == 2.2) point_ppc = new PPC_v2_2();

						float geo[3] = { X,Y,Z };
						point_ppc->SetGeometry(geo);
						if (version == 2.1) point_ppc->SetRefColor(color, cam);
						else if (version == 1.0 || version == 2.2) point_ppc->SetColor(color, cam);

						Plen_PC.push_back(point_ppc);
					}
					else {

						if (colorspace == 0) { //YUV
							int sub_V = color[2] - Plen_PC[point_idx]->GetColor(nearCamNum)[0];
							int sub_U = color[1] - Plen_PC[point_idx]->GetColor(nearCamNum)[1];

							// color thres
							if (abs(sub_U) < c_threshold && abs(sub_V) < c_threshold) {
								Plen_PC[point_idx]->SetColor(color, cam);
							}
							else {
								PPC* point_ppc;
								if (version == 1.0) point_ppc = new PPC_v1();
								else if (version == 2.1) point_ppc = new PPC_v2_1();
								else if (version == 2.2) point_ppc = new PPC_v2_2();

								float geo[3] = { X,Y,Z };
								point_ppc->SetGeometry(geo);
								if (version == 2.1) point_ppc->SetRefColor(color, cam);
								else if (version == 1.0 || version == 2.2) point_ppc->SetColor(color, cam);

								Plen_PC.push_back(point_ppc);
							}
						}
						else if (colorspace == 1) { //HSV

							Mat hsv_org(1, 1, CV_8UC3);
							hsv_org.at<Vec3b>(0, 0) = color;
							cvtColor(hsv_org, hsv_org, CV_YUV2BGR);
							cvtColor(hsv_org, hsv_org, CV_BGR2HSV);

							Mat hsv_ppc(1, 1, CV_8UC3);
							hsv_ppc.at<Vec3b>(0, 0)[0] = Plen_PC[point_idx]->GetColor(nearCamNum)[2];
							hsv_ppc.at<Vec3b>(0, 0)[1] = Plen_PC[point_idx]->GetColor(nearCamNum)[1];
							hsv_ppc.at<Vec3b>(0, 0)[2] = Plen_PC[point_idx]->GetColor(nearCamNum)[0];
							cvtColor(hsv_ppc, hsv_ppc, CV_YUV2BGR);
							cvtColor(hsv_ppc, hsv_ppc, CV_BGR2HSV);

							int sub_H = hsv_org.at<Vec3b>(0, 0)[0] - hsv_ppc.at<Vec3b>(0, 0)[0];
							int sub_S = hsv_org.at<Vec3b>(0, 0)[1] - hsv_ppc.at<Vec3b>(0, 0)[1];

							// color thres
							if (abs(sub_H) < c_threshold && abs(sub_S) < c_threshold) {
								Plen_PC[point_idx]->SetColor(color, cam);
							}
							else {
								PPC* point_ppc;
								if (version == 1.0) point_ppc = new PPC_v1();
								else if (version == 2.1) point_ppc = new PPC_v2_1();
								else if (version == 2.2) point_ppc = new PPC_v2_2();

								float geo[3] = { X,Y,Z };
								point_ppc->SetGeometry(geo);
								if (version == 2.1) point_ppc->SetRefColor(color, cam);
								else if (version == 1.0 || version == 2.2) point_ppc->SetColor(color, cam);

								Plen_PC.push_back(point_ppc);
							}
						}
						else if (colorspace == 2) { //RGB

							Mat bgr_org(1, 1, CV_8UC3);
							bgr_org.at<Vec3b>(0, 0) = color;
							cvtColor(bgr_org, bgr_org, CV_YUV2BGR);

							Mat bgr_ppc(1, 1, CV_8UC3);
							bgr_ppc.at<Vec3b>(0, 0)[0] = Plen_PC[point_idx]->GetColor(nearCamNum)[2];
							bgr_ppc.at<Vec3b>(0, 0)[1] = Plen_PC[point_idx]->GetColor(nearCamNum)[1];
							bgr_ppc.at<Vec3b>(0, 0)[2] = Plen_PC[point_idx]->GetColor(nearCamNum)[0];
							cvtColor(bgr_ppc, bgr_ppc, CV_YUV2BGR);

							int sub_B = bgr_org.at<Vec3b>(0, 0)[0] - bgr_ppc.at<Vec3b>(0, 0)[0];
							int sub_G = bgr_org.at<Vec3b>(0, 0)[1] - bgr_ppc.at<Vec3b>(0, 0)[1];
							int sub_R = bgr_org.at<Vec3b>(0, 0)[2] - bgr_ppc.at<Vec3b>(0, 0)[2];

							// color thres
							if (abs(sub_B) < c_threshold && abs(sub_G) < c_threshold && abs(sub_R) < c_threshold) {
								Plen_PC[point_idx]->SetColor(color, cam);
							}
							else {
								PPC* point_ppc;
								if (version == 1.0) point_ppc = new PPC_v1();
								else if (version == 2.1) point_ppc = new PPC_v2_1();
								else if (version == 2.2) point_ppc = new PPC_v2_2();

								float geo[3] = { X,Y,Z };
								point_ppc->SetGeometry(geo);
								if (version == 2.1) point_ppc->SetRefColor(color, cam);
								else if (version == 1.0 || version == 2.2) point_ppc->SetColor(color, cam);

								Plen_PC.push_back(point_ppc);
							}
						}
					}
				}
			}
		}

		//cout << "max_dist : " << max_dist << endl;
	}

	Plen_PC.shrink_to_fit();

	return Plen_PC;
}

//vector<PPC*> make_sequenced_Plen_PC(
//	vector<Mat> color_imgs,
//	vector<Mat> depth_imgs,
//	int colorspace,
//	vector<int> camera_order)
//{
//	vector<PPC*> Plen_PC;
//	int c_threshold = 256; //7;
//
//	PointCloud<PointXYZRGB>::Ptr firstPC;
//	if (version == 2.1) {
//		firstPC = make_PC(0, color_imgs[0], depth_imgs[0]);
//	}
//	else if (version == 2.2) {
//		firstPC = make_PC(0, color_imgs[0], depth_imgs[0]);
//	}
//
//	// 첫번째 view 
//	for (int point_idx = 0; point_idx < firstPC->points.size(); point_idx++) {
//		PPC* point_ppc;
//
//		if(version==2.1) point_ppc = new PPC_v2_1();
//		else if(version == 2.2) point_ppc = new PPC_v2_2();
//		
//		point_ppc->SetGeometry(firstPC->points[point_idx]);
//
//		if(version == 2.1 || version == 3.1) point_ppc->SetRefColor(firstPC->points[point_idx], 0);
//		else if(version == 2.2 || version == 3.2) point_ppc->SetColor(firstPC->points[point_idx], 0);  
//
//		Plen_PC.push_back(point_ppc);
//	}
//
//	for (int i = 1; i < total_num_cameras; i++) {
//		int cam = i;// camera_order[i];
//
//		int u;
//		int v;
//
//		double w;
//
//		Mat color_img(_height, _width, CV_8UC3, Scalar(0));
//		Mat confirm_img(_height, _width, CV_32S, -1);
//
//		Vec3b d;
//		Vec3s d_s;
//
//		for (int point_idx = 0; point_idx < Plen_PC.size(); point_idx++) {
//			float* geometry = Plen_PC[point_idx]->GetGeometry();
//
//			double X = geometry[0];
//			double Y = geometry[1];
//			double Z = geometry[2];
//
//			w = projection_XYZ_2_UV(
//				m_CalibParams[cam].m_ProjMatrix,
//				X,
//				Y,
//				Z,
//				u,
//				v); 
//
//			if ((u < 0) || (v < 0) || (u > _width - 1) || (v > _height - 1)) continue;
//
//			confirm_img.at<int>(v, u) = point_idx;
//		}
//		
//		for (int v = 0; v < _height; v++) {
//			for (int u = 0; u < _width; u++) {
//				Vec3b color = color_imgs[cam].at<Vec3b>(v, u);
//
//				double X;
//				double Y;
//				double Z;
//
//				switch (data_mode) {
//				case 0:
//					d = depth_imgs[cam].at<Vec3b>(v, u);
//					Z = depth_level_2_Z(d[0]);
//					break;
//				case 1:
//				case 2:
//					d_s = depth_imgs[cam].at<Vec3s>(v, u);
//					Z = depth_level_2_Z_s(d_s[0]);
//					break;
//				case 3:
//					d_s = depth_imgs[cam].at<Vec3s>(v, u);
//					Z = depth_level_2_Z_s(d_s[0], cam);
//					break;
//				case 4:
//				case 5:
//				case 6:
//				case 7:
//				case 8:
//				case 9:
//				case 10:
//				case 11:
//				case 12:
//				case 13:
//					Z = depth_level_2_Z_s_direct(depth_imgs[cam].at<ushort>(v, u));
//					break;
//				}
//
//				if (!data_mode) projection_UVZ_2_XY_PC(m_CalibParams[cam].m_ProjMatrix, u, v, Z, &X, &Y);
//				else Z = MVG(m_CalibParams[cam].m_K, m_CalibParams[cam].m_RotMatrix, m_CalibParams[cam].m_Trans, u, v, Z, &X, &Y);
//
//				// projection ok
//				if (confirm_img.at<int>(v, u) == -1) {
//					
//					PPC* point_ppc;
//					if (version == 2.1 ) point_ppc = new PPC_v2_1();
//					else if (version == 2.2 ) point_ppc = new PPC_v2_2();
//
//					float geo[3] = { X,Y,Z };
//					point_ppc->SetGeometry(geo);
//					if (version == 2.1) point_ppc->SetRefColor(color, cam);
//					else if (version == 2.2) point_ppc->SetColor(color, cam);
//
//					Plen_PC.push_back(point_ppc);
//				}
//				// projection not ok
//				else
//				{
//					int point_idx = confirm_img.at<int>(v, u);
//
//					int nearCamNum = 0;
//					for (int c = cam - 1; c >= 0; c--) {
//						if (!Plen_PC[point_idx]->CheckOcclusion(c)) {
//							nearCamNum = c;
//							break;
//						}
//					}
//
//					if (colorspace == 0) { //YUV
//						int sub_V = color[2] - Plen_PC[point_idx]->GetColor(nearCamNum)[0];
//						int sub_U = color[1] - Plen_PC[point_idx]->GetColor(nearCamNum)[1];
//
//						// color thres
//						if (abs(sub_U) < c_threshold && abs(sub_V) < c_threshold) {
//							Plen_PC[point_idx]->SetColor(color, cam);
//						}
//						else {
//							PPC* point_ppc;
//							if (version == 2.1) point_ppc = new PPC_v2_1();
//							else if (version == 2.2) point_ppc = new PPC_v2_2();
//
//							float geo[3] = { X,Y,Z };
//							point_ppc->SetGeometry(geo);
//							if (version == 2.1) point_ppc->SetRefColor(color, cam);
//							else if (version == 2.2) point_ppc->SetColor(color, cam);
//
//							Plen_PC.push_back(point_ppc);
//						}
//					}
//					else if (colorspace == 1) { //HSV
//
//						Mat hsv_org(1, 1, CV_8UC3);
//						hsv_org.at<Vec3b>(0, 0) = color;
//						cvtColor(hsv_org, hsv_org, CV_YUV2BGR);
//						cvtColor(hsv_org, hsv_org, CV_BGR2HSV);
//
//						Mat hsv_ppc(1, 1, CV_8UC3);
//						hsv_ppc.at<Vec3b>(0, 0)[0] = Plen_PC[point_idx]->GetColor(nearCamNum)[2];
//						hsv_ppc.at<Vec3b>(0, 0)[1] = Plen_PC[point_idx]->GetColor(nearCamNum)[1];
//						hsv_ppc.at<Vec3b>(0, 0)[2] = Plen_PC[point_idx]->GetColor(nearCamNum)[0];
//						cvtColor(hsv_ppc, hsv_ppc, CV_YUV2BGR);
//						cvtColor(hsv_ppc, hsv_ppc, CV_BGR2HSV);
//
//						int sub_H = hsv_org.at<Vec3b>(0, 0)[0] - hsv_ppc.at<Vec3b>(0, 0)[0];
//						int sub_S = hsv_org.at<Vec3b>(0, 0)[1] - hsv_ppc.at<Vec3b>(0, 0)[1];
//
//						// color thres
//						if (abs(sub_H) < c_threshold && abs(sub_S) < c_threshold) {
//							Plen_PC[point_idx]->SetColor(color, cam);
//						}
//						else {
//							PPC* point_ppc;
//							if (version == 2.1 ) point_ppc = new PPC_v2_1();
//							else if (version == 2.2 ) point_ppc = new PPC_v2_2();
//
//							float geo[3] = { X,Y,Z };
//							point_ppc->SetGeometry(geo);
//							if (version == 2.1) point_ppc->SetRefColor(color, cam);
//							else if (version == 2.2) point_ppc->SetColor(color, cam);
//
//							Plen_PC.push_back(point_ppc);
//						}
//
//					}
//					else if (colorspace == 2) { //RGB
//
//						Mat bgr_org(1, 1, CV_8UC3);
//						bgr_org.at<Vec3b>(0, 0) = color;
//						cvtColor(bgr_org, bgr_org, CV_YUV2BGR);
//
//						Mat bgr_ppc(1, 1, CV_8UC3);
//						bgr_ppc.at<Vec3b>(0, 0)[0] = Plen_PC[point_idx]->GetColor(nearCamNum)[2];
//						bgr_ppc.at<Vec3b>(0, 0)[1] = Plen_PC[point_idx]->GetColor(nearCamNum)[1];
//						bgr_ppc.at<Vec3b>(0, 0)[2] = Plen_PC[point_idx]->GetColor(nearCamNum)[0];
//						cvtColor(bgr_ppc, bgr_ppc, CV_YUV2BGR);
//
//						int sub_B = bgr_org.at<Vec3b>(0, 0)[0] - bgr_ppc.at<Vec3b>(0, 0)[0];
//						int sub_G = bgr_org.at<Vec3b>(0, 0)[1] - bgr_ppc.at<Vec3b>(0, 0)[1];
//						int sub_R = bgr_org.at<Vec3b>(0, 0)[2] - bgr_ppc.at<Vec3b>(0, 0)[2];
//
//						// color thres
//						if (abs(sub_B) < c_threshold && abs(sub_G) < c_threshold && abs(sub_R) < c_threshold) {
//							Plen_PC[point_idx]->SetColor(color, cam);
//						}
//						else {
//							PPC* point_ppc;
//							if (version == 2.1 || version == 3.1) point_ppc = new PPC_v2_1();
//							else if (version == 2.2 || version == 3.2) point_ppc = new PPC_v2_2();
//
//							float geo[3] = { X,Y,Z };
//							point_ppc->SetGeometry(geo);
//							if (version == 2.1 || version == 3.1) point_ppc->SetRefColor(color, cam);
//							else if (version == 2.2 || version == 3.2) point_ppc->SetColor(color, cam);
//
//							Plen_PC.push_back(point_ppc);
//						}
//					}
//				}
//			}
//		}
//	}
//	
//	Plen_PC.shrink_to_fit();
//
//	return Plen_PC;
//}

vector<PPC*> make_voxelized_Plen_PC(
	vector<PointCloud<PointXYZRGB>::Ptr> pointclouds,
	int voxel_div_num) {

	vector<float> min(3), max(3);

	find_min_max(pointclouds, min, max);

	float x_size = (max[0] - min[0]);
	float y_size = (max[1] - min[1]);
	float z_size = (max[2] - min[2]);

	unsigned long long x_voxel_num = voxel_div_num;
	unsigned long long y_voxel_num = voxel_div_num;
	unsigned long long z_voxel_num = voxel_div_num;

	unsigned long long total_voxel_num = x_voxel_num * y_voxel_num * z_voxel_num;

	float x_stride = x_size / x_voxel_num;
	float y_stride = y_size / y_voxel_num;
	float z_stride = z_size / z_voxel_num;

	// color cube, 카메라 별로 색을 쌓아논거
	map< unsigned long long, vector<vector<Vec3b>> > color_map;
	for (int cam_num = 0; cam_num < total_num_cameras; cam_num++) {
		for (unsigned long long point_idx = 0; point_idx < pointclouds[cam_num]->points.size(); point_idx++) {
			unsigned long long  x_voxel_index = (int)floor((pointclouds[cam_num]->points[point_idx].x - min[0]) / x_size * ((float)x_voxel_num - 1));
			unsigned long long  y_voxel_index = (int)floor((pointclouds[cam_num]->points[point_idx].y - min[1]) / y_size * ((float)y_voxel_num - 1));
			unsigned long long  z_voxel_index = (int)floor((pointclouds[cam_num]->points[point_idx].z - min[2]) / z_size * ((float)z_voxel_num - 1));
			unsigned long long cube_index = x_voxel_index * (y_voxel_num * z_voxel_num) + y_voxel_index * z_voxel_num + z_voxel_index;

			// find(key를 찾는거)를 했을때 return이 end()이면 해당 key는 map에 없음
			// 없으면(기존에 할당되지 않은 큐브 인덱스) 새로 추가
			if (color_map.find(cube_index) == color_map.end()) {
				vector<vector<Vec3b>> temp_color(total_num_cameras);
				temp_color[cam_num].push_back(Vec3b(pointclouds[cam_num]->points[point_idx].r,
					pointclouds[cam_num]->points[point_idx].g,
					pointclouds[cam_num]->points[point_idx].b));
				color_map.insert(color_map.end(), pair< unsigned long long, vector<vector<Vec3b>> >(cube_index, temp_color));
			}
			// 기존 key 중에 존재하면, 해당 카메라에 색을 쌓음.
			else {
				color_map[cube_index][cam_num].push_back(Vec3b(pointclouds[cam_num]->points[point_idx].r,
					pointclouds[cam_num]->points[point_idx].g,
					pointclouds[cam_num]->points[point_idx].b));
			}
		}
	}

	vector<PPC*> Plen_PC;

	// map의 iterator 코드
	// 색의 평균을 구하는 부분
	// first = key, second = value

	for (map<unsigned long long, vector<vector<Vec3b>> >::iterator it = color_map.begin(); it != color_map.end(); ++it) {
		PPC* temp_PP;
		if (version == 2.1) temp_PP = new PPC_v2_1();
		else if (version == 2.2) temp_PP = new PPC_v2_2();

		int inter_r = 0, inter_g = 0, inter_b = 0;
		int inter_cnt = 0;

		for (int cam_num = 0; cam_num < total_num_cameras; cam_num++) {
			if (it->second[cam_num].size() > 0) {
				int avg_r = 0, avg_g = 0, avg_b = 0;
				for (unsigned long long i = 0; i < it->second[cam_num].size(); i++) {
					avg_r += int(it->second[cam_num][i][0]);
					avg_g += int(it->second[cam_num][i][1]);
					avg_b += int(it->second[cam_num][i][2]);
				}

				avg_r /= it->second[cam_num].size();
				avg_g /= it->second[cam_num].size();
				avg_b /= it->second[cam_num].size();

				if (version == 2.1) {
					if (inter_cnt == 0) {
						temp_PP->SetRefColor(Vec3b((uchar)avg_b, (uchar)avg_g, (uchar)avg_r), cam_num);
					}
					else {
						temp_PP->SetColor((uchar)avg_r, (uchar)avg_g, (uchar)avg_b, cam_num);
					}
				}

				//temp_PP.color[cam_num][0] = (short)avg_r;
				//temp_PP.color[cam_num][1] = (short)avg_g;
				//temp_PP.color[cam_num][2] = (short)avg_b;

				// 색 보간
				inter_r += avg_r;
				inter_g += avg_g;
				inter_b += avg_b;
				inter_cnt++;
			}
		}
		inter_r /= inter_cnt;
		inter_g /= inter_cnt;
		inter_b /= inter_cnt;

		// 보간 색 할당
		//for (int cam_num = 0; cam_num < total_num_cameras; cam_num++) {
		//	if (it->second[cam_num].size() == 0) {
		//		//it->second[cam_num].push_back(Vec3b((uchar)inter_r, (uchar)inter_g, (uchar)inter_b));
		//		temp_PP.color[cam_num][0] = inter_r;
		//		temp_PP.color[cam_num][1] = inter_g;
		//		temp_PP.color[cam_num][2] = inter_b;
		//		temp_PP.occlusion_pattern[cam_num] = false;
		//	}

		//	else
		//		temp_PP.occlusion_pattern[cam_num] = true;
		//}

		int x_idx = it->first / (y_voxel_num * z_voxel_num);
		int y_idx = (it->first % (y_voxel_num * z_voxel_num)) / z_voxel_num;
		int z_idx = (it->first % (y_voxel_num * z_voxel_num)) % z_voxel_num;

		float geo[3] = { (float(x_idx) / (x_voxel_num - 1) * x_size) + min[0] + (x_stride / 2) ,
						(float(y_idx) / (y_voxel_num - 1) * y_size) + min[1] + (y_stride / 2) ,
						(float(z_idx) / (z_voxel_num - 1) * z_size) + min[2] + (z_stride / 2) };
		temp_PP->SetGeometry(geo);

		Plen_PC.push_back(temp_PP);
	}

	color_map.clear();

	return Plen_PC;
}

vector<PPC*> make_voxelized_Plen_PC(
	vector<PPC*> PPC_vec,
	int voxel_div_num,
	vector<float> &Cube_size,
	vector<float> &cube_size) {

	vector<float> min(3), max(3);
	PointCloud<PointXYZRGB>::Ptr pointcloud_ppc(new PointCloud<PointXYZRGB>);

	for (int i = 0; i < PPC_vec.size(); i++) {
		PointXYZRGB p;
		float* geo = PPC_vec[i]->GetGeometry();
		p.x = geo[0];
		p.y = geo[1];
		p.z = geo[2];
		p.r = 0;
		p.g = 0;
		p.b = 0;

		pointcloud_ppc->points.push_back(p);
	}

	find_min_max(pointcloud_ppc, min, max);

	float x_size = (max[0] - min[0]);
	float y_size = (max[1] - min[1]);
	float z_size = (max[2] - min[2]);

	unsigned long long x_voxel_num = voxel_div_num;
	unsigned long long y_voxel_num = voxel_div_num;
	unsigned long long z_voxel_num = voxel_div_num;

	unsigned long long total_voxel_num = x_voxel_num * y_voxel_num * z_voxel_num;

	float x_stride = x_size / x_voxel_num;
	float y_stride = y_size / y_voxel_num;
	float z_stride = z_size / z_voxel_num;

	cout << "Voxelized Cube_x_size : " << x_size << "\tCube_y_size : " << y_size << "\tCube_z_size : " << z_size << endl;
	cout << "Voxelized cube_x_size : " << x_stride << "\tcube_y_size : " << y_stride << "\tcube_z_size : " << z_stride << endl;
	
	Cube_size = { x_size, y_size, z_size };
	cube_size = { x_stride, y_stride, z_stride };

	map<unsigned long long, PPC*> ppc_in_voxels_map;
	map<unsigned long long, vector<char>> numberOfcolor_inVoxels;

	for (int point_idx = 0; point_idx < PPC_vec.size(); point_idx++) {

		float* geo = PPC_vec[point_idx]->GetGeometry();
		unsigned long long x_voxel_index = (int)floor((geo[0] - min[0]) / x_size * ((float)x_voxel_num - 1));
		unsigned long long y_voxel_index = (int)floor((geo[1] - min[1]) / y_size * ((float)y_voxel_num - 1));
		unsigned long long z_voxel_index = (int)floor((geo[2] - min[2]) / z_size * ((float)z_voxel_num - 1));

		unsigned long long cube_index = x_voxel_index * (y_voxel_num * z_voxel_num) + y_voxel_index * z_voxel_num + z_voxel_index;

		//if (numberOfcolor_inVoxels.find(cube_index) == numberOfcolor_inVoxels.end())
		//	numberOfcolor_inVoxels.insert(numberOfcolor_inVoxels.end(), pair<unsigned long long, vector<char>>(cube_index, vector<char>(total_num_cameras, char(0))));

		//for (int cam = 0; cam < total_num_cameras; cam++) {
		//	if (PPC_vec[point_idx]->CheckOcclusion(cam)) continue;

		//	if (numberOfcolor_inVoxels[cube_index][cam] != char(0)) {

		//		//색- 평균값
		//		Vec3b color_cur = PPC_vec[point_idx]->GetColor(cam);
		//		int numColor = int(numberOfcolor_inVoxels[cube_index][cam]);
		//		Vec3b color_prev = ppc_in_voxels_map[cube_index]->GetColor(cam);

		//		color_cur[0] = float(color_prev[0]) * numColor / (numColor + 1) + float(color_cur[0]) / (numColor + 1);
		//		color_cur[1] = float(color_prev[1]) * numColor / (numColor + 1) + float(color_cur[1]) / (numColor + 1);
		//		color_cur[2] = float(color_prev[2]) * numColor / (numColor + 1) + float(color_cur[2]) / (numColor + 1);
		//		numberOfcolor_inVoxels[cube_index][cam]++;
		//		ppc_in_voxels_map[cube_index]->SetColor(color_cur, cam);
		//	}

		//	//복셀 내의 첫번째 점
		//	else {

		//		numberOfcolor_inVoxels[cube_index][cam]++;

		//		float geo2[3] = { (float(x_voxel_index) / (x_voxel_num - 1) * x_size) + min[0] + (x_stride / 2) ,
		//				 (float(y_voxel_index) / (y_voxel_num - 1) * y_size) + min[1] + (y_stride / 2) ,
		//				 (float(z_voxel_index) / (z_voxel_num - 1) * z_size) + min[2] + (z_stride / 2) };
		//		PPC_vec[point_idx]->SetGeometry(geo2);

		//		ppc_in_voxels_map.insert(ppc_in_voxels_map.end(), pair<unsigned long long, PPC*>(cube_index, PPC_vec[point_idx]));

		//	}
		//}

		//복셀 내 첫번째 점일 경우
		if (numberOfcolor_inVoxels.find(cube_index) == numberOfcolor_inVoxels.end()) {
			numberOfcolor_inVoxels.insert(numberOfcolor_inVoxels.end(), pair<unsigned long long, vector<char>>(cube_index, vector<char>(total_num_cameras, char(0))));

			float geo2[3] = { (float(x_voxel_index) / (x_voxel_num - 1) * x_size) + min[0] + (x_stride / 2) ,
						 (float(y_voxel_index) / (y_voxel_num - 1) * y_size) + min[1] + (y_stride / 2) ,
						 (float(z_voxel_index) / (z_voxel_num - 1) * z_size) + min[2] + (z_stride / 2) };
			PPC_vec[point_idx]->SetGeometry(geo2);

			ppc_in_voxels_map.insert(ppc_in_voxels_map.end(), pair<unsigned long long, PPC*>(cube_index, PPC_vec[point_idx]));

		}

		for (int cam = 0; cam < total_num_cameras; cam++) {
			if (!PPC_vec[point_idx]->CheckOcclusion(cam)) {
				Vec3b color_curr = PPC_vec[point_idx]->GetColor(cam);
				Vec3b color_cur;
				color_cur[0] = color_curr[2];
				color_cur[1] = color_curr[1];
				color_cur[2] = color_curr[0];
				if (int(numberOfcolor_inVoxels[cube_index][cam]) != 0) {

					//색- 평균값
					int numColor = int(numberOfcolor_inVoxels[cube_index][cam]);
					Vec3b color_prevv = ppc_in_voxels_map[cube_index]->GetColor(cam);
					Vec3b color_prev;
					color_prev[0] = color_prevv[2];
					color_prev[1] = color_prevv[1];
					color_prev[2] = color_prevv[0];

					color_cur[0] = color_prev[0] * numColor / (numColor + 1) + color_cur[0] / (numColor + 1);
					color_cur[1] = color_prev[1] * numColor / (numColor + 1) + color_cur[1] / (numColor + 1);
					color_cur[2] = color_prev[2] * numColor / (numColor + 1) + color_cur[2] / (numColor + 1);
					numberOfcolor_inVoxels[cube_index][cam]++;
					ppc_in_voxels_map[cube_index]->SetColor(color_cur, cam);

				}
				else {
					//cout << (int)color_cur[0] << "\t" << (int)color_cur[1] << "\t" << (int)color_cur[2] << endl;
					numberOfcolor_inVoxels[cube_index][cam]++;
					ppc_in_voxels_map[cube_index]->SetColor(color_cur, cam);
				}
			}
		}
	}

	//cout << "valid voxel size : " << numberOfcolor_inVoxels.size() << endl;

	//for (map<unsigned long long, vector<char>>::iterator it = numberOfcolor_inVoxels.begin(); it != numberOfcolor_inVoxels.end(); it++) {
	//	for (int i = 0; i < total_num_cameras; i++)
	//		if(int(it->second[i]) !=0 && int(it->second[i]) != 1)
	//		cout << int(it->second[i]) <<" ";
	//	//cout << endl <<  "====" << endl;
	//}
	numberOfcolor_inVoxels.clear();

	vector<PPC*> voxelized_PPC;
	for (auto it = ppc_in_voxels_map.begin(); it != ppc_in_voxels_map.end(); it++) {
		voxelized_PPC.push_back(it->second);
	}

	//ppc_in_voxels_map.clear();

	return voxelized_PPC;
}

vector<PPC*> make_formulaic_voxelized_Plen_PC(
	vector<Mat> color_imgs,
	vector<Mat> depth_imgs,
	int voxel_div_num) {

	vector<PPC*> Plen_PC = make_incremental_Plen_PC(color_imgs, depth_imgs, 0, camera_order);

	vector<float> min(3), max(3);
	find_min_max(Plen_PC, min, max);

	Plen_PC.clear();

	/////////////
	version = 1.0;

	float Cube_x_size = max[0] - min[0];
	float Cube_y_size = max[1] - min[1];
	float Cube_z_size = max[2] - min[2];

	float cube_x_size = Cube_x_size / voxel_div_num;
	float cube_y_size = Cube_y_size / voxel_div_num;
	float cube_z_size = Cube_z_size / voxel_div_num;

	cout << "Cube_x_size : " << Cube_x_size << "\tCube_y_size : " << Cube_y_size << "\tCube_z_size : " << Cube_z_size << endl;
	cout << "cube_x_size : " << cube_x_size << "\tcube_y_size : " << cube_y_size << "\tcube_z_size : " << cube_z_size << endl;

	/////////////////////////////////////////////
	double depth_threshold = sqrt((cube_x_size * cube_x_size) + (cube_y_size * cube_y_size) + (cube_z_size * cube_z_size)) * 0.5;
	/////////////////////////////////////////////

	Mat confirm_img(_height, _width, CV_8UC1, Scalar(0));
	vector<Mat> confirm_imgs(total_num_cameras, confirm_img);

	vector<PPC*> ppc_vec;

	double max_max_dist = 0.;
	for (int x_idx = 0; x_idx < voxel_div_num; x_idx++) {
		for (int y_idx = 0; y_idx < voxel_div_num; y_idx++) {
			for (int z_idx = 0; z_idx < voxel_div_num; z_idx++) {
				
				bool is_first_color = true;
				PPC* point_ppc;

				float X = min[0] + (x_idx * cube_x_size) + (cube_x_size / 2.0);
				float Y = min[1] + (y_idx * cube_y_size) + (cube_y_size / 2.0);
				float Z = min[2] + (z_idx * cube_z_size) + (cube_z_size / 2.0);

				
				double max_dist = 0.; 
				for (int cam = 0; cam < total_num_cameras; cam++) {

					PointXYZ p;

					p.x = X;
					p.y = Y;
					p.z = Z;

					int u, v;

					double w = projection_XYZ_2_UV(
						m_CalibParams[cam].m_ProjMatrix,
						(double)X,
						(double)Y,
						(double)Z,
						u,
						v);

					//복셀의 중점과 image plane과의 거리값 계산
					double dist_point2camera = find_point_dist(w, cam);

					if ((u < 0) || (v < 0) || (u > _width - 1) || (v > _height - 1)) continue;

					//confirm_img로 색상할당 판별
					//if (confirm_imgs[cam].at<uchar>(v, u) != uchar(0)) continue;


					//원본거리값 계산
					double X_origin, Y_origin, Z_origin;
					switch (data_mode) {
					case 0:

						Z_origin = depth_level_2_Z(depth_imgs[cam].at<Vec3b>(v, u)[0]);
						break;

					case 1: case 2: case 3:

						Z_origin = depth_level_2_Z_s(depth_imgs[cam].at<Vec3s>(v, u)[0]);
						break;

					case 4: case 5: case 6: case 7: case 8: case 9:
					case 10: case 11: case 12: case 13:
						Z_origin = depth_level_2_Z_s_direct(depth_imgs[cam].at<ushort>(v, u));
						break;
					}
					if (!data_mode) projection_UVZ_2_XY_PC(m_CalibParams[cam].m_ProjMatrix, u, v, Z_origin, &X_origin, &Y_origin);
					else Z_origin = MVG(m_CalibParams[cam].m_K, m_CalibParams[cam].m_RotMatrix, m_CalibParams[cam].m_Trans, u, v, Z_origin, &X_origin, &Y_origin);

					double w_origin = m_CalibParams[cam].m_ProjMatrix(2, 0)* X_origin + m_CalibParams[cam].m_ProjMatrix(2, 1) * Y_origin + m_CalibParams[cam].m_ProjMatrix(2, 2) * Z_origin + m_CalibParams[cam].m_ProjMatrix(2, 3);
					//double w_origin = projection_XYZ_2_UV(m_CalibParams[cam].m_ProjMatrix, X_origin, Y_origin, Z_origin, u, v);
					double dist_origin = find_point_dist(w_origin, cam);

					//if (max_dist < abs(dist_origin - dist_point2camera)) max_dist = abs(dist_origin - dist_point2camera);

					//cout << "dist_origin :\t" << dist_origin << "\tdist_point2camera :\t" << dist_point2camera << "\tsub :\t" << abs(dist_origin - dist_point2camera) << endl;
					
					// 원본 거리값과 복셀의 중점과의 거리 오차로 색상할당 판별 
					if (abs(dist_origin - dist_point2camera) < depth_threshold) {

						//confirm_imgs[cam].at<uchar>(v, u) = uchar(1);

						if (is_first_color) {
							float geo[3] = { X, Y, Z };
							if (version == 2.1) {
								point_ppc = new PPC_v2_1();

								point_ppc->SetGeometry(geo);
								point_ppc->SetRefColor(color_imgs[cam].at<Vec3b>(v, u), cam);
							}
							else if (version == 2.2) { 
								point_ppc = new PPC_v2_2();

								point_ppc->SetGeometry(geo);
								point_ppc->SetColor(color_imgs[cam].at<Vec3b>(v, u), cam);
							}
							else if (version == 1.0) {
								point_ppc = new PPC_v1();

								point_ppc->SetGeometry(geo);
								point_ppc->SetColor(color_imgs[cam].at<Vec3b>(v, u), cam);
							}
							is_first_color = false;
						}
						else {
							point_ppc->SetColor(color_imgs[cam].at<Vec3b>(v, u), cam);
						}
					}
				}
				if (max_dist > max_max_dist) max_max_dist = max_dist;
				if(!is_first_color)	ppc_vec.push_back(point_ppc);
			}
		}
	}
	cout << "max_max_dist : " << max_max_dist << endl;

	return ppc_vec;
}

vector<PPC*> make_formulaic_voxelized_Plen_PC2(
	vector<Mat> color_imgs,
	vector<Mat> depth_imgs,
	int voxel_div_num,
	vector<float> &Cube_size,
	vector<float> &cube_size) {

	//vector<PPC*> Plen_PC = make_sequenced_Plen_PC(color_imgs, depth_imgs, 0, camera_order);

	vector<PointCloud<PointXYZRGB>::Ptr> point_clouds = make_all_PC(color_imgs, depth_imgs);

	vector<float> min(3), max(3);
	//find_min_max(Plen_PC, min, max);
	find_min_max(point_clouds, min, max);

	/////////////
	//version = 1.0;

	float Cube_x_size = max[0] - min[0];
	float Cube_y_size = max[1] - min[1];
	float Cube_z_size = max[2] - min[2];

	float cube_x_size = Cube_x_size / voxel_div_num;
	float cube_y_size = Cube_y_size / voxel_div_num;
	float cube_z_size = Cube_z_size / voxel_div_num;

	cout << "Cube_x_size : " << Cube_x_size << "\t\tCube_y_size : " << Cube_y_size << "\t\tCube_z_size : " << Cube_z_size << endl;
	cout << "cube_x_size : " << cube_x_size << "\t\tcube_y_size : " << cube_y_size << "\t\tcube_z_size : " << cube_z_size << endl;

	Cube_size = { Cube_x_size, Cube_y_size, Cube_z_size };
	cube_size = { cube_x_size, cube_y_size, cube_z_size };

	set<unsigned long long> valid_cube_indices;
	unsigned long long voxel_div_num_ = voxel_div_num;

	//////////incremental PPC///////
	//cout << "incremental ppc size : " << Plen_PC.size() << endl;
	//for (int point_idx = 0; point_idx < Plen_PC.size(); point_idx++) {

	//	float* geo = Plen_PC[point_idx]->GetGeometry();
	//	unsigned long long x_voxel_index = (int)floor((geo[0] - min[0]) / Cube_x_size * ((float)voxel_div_num - 1));
	//	unsigned long long y_voxel_index = (int)floor((geo[1] - min[1]) / Cube_y_size * ((float)voxel_div_num - 1));
	//	unsigned long long z_voxel_index = (int)floor((geo[2] - min[2]) / Cube_z_size * ((float)voxel_div_num - 1));

	//	unsigned long long cube_index = x_voxel_index * (voxel_div_num_ * voxel_div_num_) + y_voxel_index * voxel_div_num_ + z_voxel_index;
	//	valid_cube_indices.insert(cube_index);
	//}
	//Plen_PC.clear();
	////////////////////////////////


	////////////////////all PC//////////////////
	unsigned long long x_voxel_index;
	unsigned long long y_voxel_index;
	unsigned long long z_voxel_index;
	unsigned long long cube_index;
	int count = 0;
	PointXYZRGB p;

	for (int pc_idx = 0; pc_idx < point_clouds.size(); pc_idx++) {
		//cout << "pc_idx : " << pc_idx << endl;
		count += point_clouds.size();
		PointCloud<PointXYZRGB>::Ptr pc = point_clouds[pc_idx];
		for (int p_idx = 0; p_idx < pc->points.size(); p_idx++) {

			p = pc->points[p_idx];
			x_voxel_index = (int)floor((p.x - min[0]) / Cube_x_size * ((float)voxel_div_num - 1));
			y_voxel_index = (int)floor((p.y - min[1]) / Cube_y_size * ((float)voxel_div_num - 1));
			z_voxel_index = (int)floor((p.z - min[2]) / Cube_z_size * ((float)voxel_div_num - 1));

			cube_index = x_voxel_index * (voxel_div_num_ * voxel_div_num_) + y_voxel_index * voxel_div_num_ + z_voxel_index;
			valid_cube_indices.insert(cube_index);
		}
	}
	point_clouds.clear();
	vector<PointCloud<PointXYZRGB>::Ptr>().swap(point_clouds);
	/////////////////////////////////////////////

	cout << "valid_cube_indices size : " << valid_cube_indices.size() << endl;

	/////////////////////////////////////////////
	double depth_threshold = sqrt((cube_x_size * cube_x_size) + (cube_y_size * cube_y_size) + (cube_z_size * cube_z_size));// *0.5;
	/////////////////////////////////////////////

	Mat confirm_img(_height, _width, CV_8UC1, Scalar(0));
	vector<Mat> confirm_imgs(total_num_cameras, confirm_img);

	vector<PPC*> ppc_vec;

	set<unsigned long long>::iterator cube_index_iter;
	int cnt = 0; 

	int x_idx, y_idx, z_idx;
	float X, Y, Z;

	int u, v;

	double w, dist_point2camera, w_origin, dist_origin;
	//PointXYZ p2;

	for(cube_index_iter = valid_cube_indices.begin(); cube_index_iter !=valid_cube_indices.end(); cube_index_iter++){
		if (cnt % 10000000==0) cout << cnt << "th cube is being produced . . " << endl;

		unsigned long long cube_index = *cube_index_iter;

		//(unsigned long long)32767 / (int)1073741824
		//(unsigned long long)65535 / (int)4294967296

		x_idx = int(cube_index / (voxel_div_num_ * voxel_div_num_));
		y_idx = int((cube_index % (voxel_div_num_ * voxel_div_num_)) / voxel_div_num);
		z_idx = int((cube_index % (voxel_div_num_ * voxel_div_num_)) % voxel_div_num);

		X = (float(x_idx) / (voxel_div_num - 1) * Cube_x_size) + min[0] + (cube_x_size / 2);
		Y = (float(y_idx) / (voxel_div_num - 1) * Cube_y_size) + min[1] + (cube_y_size / 2);
		Z = (float(z_idx) / (voxel_div_num - 1) * Cube_z_size) + min[2] + (cube_z_size / 2);

		//cout << "X :" << X << "\tY : " << Y << "\tZ : " << Z << endl;
		bool is_first_color = true;
		PPC* point_ppc;

		cnt++;
		double max_dist = 0.;
		for (int cam = 0; cam < total_num_cameras; cam++) {

			//p2.x = X;
			//p2.y = Y;
			//p2.z = Z;

			w = projection_XYZ_2_UV(
				m_CalibParams[cam].m_ProjMatrix,
				(double)X,
				(double)Y,
				(double)Z,
				u,
				v);

			//복셀의 중점과 image plane과의 거리값 계산
			dist_point2camera = find_point_dist(w, cam);

			if ((u < 0) || (v < 0) || (u > _width - 1) || (v > _height - 1)) continue;

			//confirm_img로 색상할당 판별
			//if (confirm_imgs[cam].at<uchar>(v, u) != uchar(0)) continue;

			//원본거리값 계산
			double X_origin, Y_origin, Z_origin;
			switch (data_mode) {
			case 0:
				Z_origin = depth_level_2_Z(depth_imgs[cam].at<Vec3b>(v, u)[0]);
				break;

			case 1: case 2: case 3:
				Z_origin = depth_level_2_Z_s(depth_imgs[cam].at<Vec3s>(v, u)[0]);
				break;

			case 4: case 5: case 6: case 7: case 8: case 9:
			case 10: case 11: case 12: case 13:
				Z_origin = depth_level_2_Z_s_direct(depth_imgs[cam].at<ushort>(v, u));
				break;
			}

			if (!data_mode) projection_UVZ_2_XY_PC(m_CalibParams[cam].m_ProjMatrix, u, v, Z_origin, &X_origin, &Y_origin);
			else Z_origin = MVG(m_CalibParams[cam].m_K, m_CalibParams[cam].m_RotMatrix, m_CalibParams[cam].m_Trans, u, v, Z_origin, &X_origin, &Y_origin);

			w_origin = m_CalibParams[cam].m_ProjMatrix(2, 0) * X_origin + m_CalibParams[cam].m_ProjMatrix(2, 1) * Y_origin + m_CalibParams[cam].m_ProjMatrix(2, 2) * Z_origin + m_CalibParams[cam].m_ProjMatrix(2, 3);
			//double w_origin = projection_XYZ_2_UV(m_CalibParams[cam].m_ProjMatrix, X_origin, Y_origin, Z_origin, u, v);
			dist_origin = find_point_dist(w_origin, cam);


			// 원본 거리값과 복셀의 중점과의 거리 오차로 색상할당 판별 
			//if (abs(dist_origin - dist_point2camera) < depth_threshold) {
				//confirm_imgs[cam].at<uchar>(v, u) = uchar(1);

			if (abs(dist_origin - dist_point2camera) < depth_threshold) {
				if (is_first_color) {
					float geo[3] = { X, Y, Z };
					if (version == 1.0) {
						point_ppc = new PPC_v1();

						point_ppc->SetGeometry(geo);
						point_ppc->SetColor(color_imgs[cam].at<Vec3b>(v, u), cam);
					}
					else if (version == 2.1) {
						point_ppc = new PPC_v2_1();

						point_ppc->SetGeometry(geo);
						point_ppc->SetRefColor(color_imgs[cam].at<Vec3b>(v, u), cam);
					}
					else if (version == 2.2) {
						point_ppc = new PPC_v2_2();

						point_ppc->SetGeometry(geo);
						point_ppc->SetColor(color_imgs[cam].at<Vec3b>(v, u), cam);
					}
					is_first_color = false;
				}
				else {
					point_ppc->SetColor(color_imgs[cam].at<Vec3b>(v, u), cam);
				}
			}
			//}

		}

		if (!is_first_color) ppc_vec.push_back(point_ppc);
	}
	valid_cube_indices.clear();
	set<unsigned long long>().swap(valid_cube_indices);

	return ppc_vec;
}

vector<PointCloud<PointXYZRGB>::Ptr> make_all_PC(
	vector<Mat> color_imgs, 
	vector<Mat> depth_imgs) {

	vector<PointCloud<PointXYZRGB>::Ptr> pointclouds;
	for (int cam = 0; cam < total_num_cameras; cam++) {
		PointCloud<PointXYZRGB>::Ptr pointcloud(new PointCloud<PointXYZRGB>);
		pointcloud = make_PC(cam, color_imgs[cam], depth_imgs[cam]);

		pointclouds.push_back(pointcloud);
	}

	return pointclouds;
}


void make_proj_img_vec_ppc(
	vector<PPC*> PPC,
	vector<Mat> &proj_img_vec,
	vector<Mat>& is_hole_proj_imgs,
	vector<PointCloud<PointXYZRGB>::Ptr> &pointclouds,
	int nNeighbor)
{
	for (int i = 0; i < total_num_cameras; i++) {
		pointclouds[i] = PointCloud<PointXYZRGB>::Ptr(new PointCloud<PointXYZRGB>);
	}

	PointCloud<PointXYZRGB>::Ptr pointcloud(new PointCloud<PointXYZRGB>);

	Mat dist_img(_height, _width, CV_64F, -1);
	vector<Mat> dist_imgs(total_num_cameras, dist_img);

	PointXYZRGB temp;

	for (int j = 0; j < total_num_cameras; j++)
	{
		Mat color_img(_height, _width, CV_8UC3, Scalar(0));
		Mat depth_value_img(_height, _width, CV_64F, -1);
		Mat is_hole_img(_height, _width, CV_8U, Scalar::all(1));

		int count = 0, non_count = 0;
		for (int i = 0; i < PPC.size(); i++)
		{
			if (!PPC[i]->CheckOcclusion(j)) {
				//geometry
				float* geo = PPC[i]->GetGeometry();
				temp.x = geo[0];
				temp.y = geo[1];
				temp.z = geo[2];

				//color
				Vec3b color = PPC[i]->GetColor(j);
				temp.r = color[0];
				temp.g = color[1];
				temp.b = color[2];

				pointcloud->points.push_back(temp);
				count++;
			}
			else { non_count++; }
		}

		//cout << j << "th valid pointcloud 개수 : " << count << endl;
		//cout << j << "th invalid pointcloud 개수 : " << non_count << endl;

		if(proj_mode == 0) projection(pointcloud, j, color_img, depth_value_img, is_hole_img);
		else if(proj_mode == 1) back_projection(pointcloud, j, color_img, is_hole_img, nNeighbor);
		proj_img_vec[j] = color_img;
		is_hole_proj_imgs[j] = is_hole_img;

		if (j % 10 == 0) cout << j << "th point cloud is being projected .." << endl;
		
		//if(j==3) view_PC_yuvTorgb(pointcloud);
		//else if(j==10) view_PC_yuvTorgb(pointcloud);
		
		pointcloud->points.clear();
	}
}

//not generate each pointclouds
void make_proj_img_vec_ppc2(
	vector<PPC*> PPC,
	vector<Mat>& proj_img_vec,
	vector<Mat>& is_hole_proj_imgs,
	vector<PointCloud<PointXYZRGB>::Ptr>& pointclouds,
	int nNeighbor)
{
	//하나의 Mat 객체로 Mat vector를 초기화한다면 vector 요소들이 같은 메모리 참조하기 때문에 이를 방지
	vector<Mat> depth_value_imgs(total_num_cameras);

	for (int i = 0; i < total_num_cameras; i++) {
		Mat color_img(_height, _width, CV_8UC3, Scalar(0));
		Mat is_hole_img(_height, _width, CV_8U, Scalar::all(1));
		Mat depth_value_img(_height, _width, CV_64F, -1);

		proj_img_vec[i] = color_img;
		is_hole_proj_imgs[i] = is_hole_img;
		depth_value_imgs[i] = depth_value_img;
	}

	PointXYZRGB p;
	float* geo;
	Vec3b color;

	for (int ppc_i = 0; ppc_i < PPC.size(); ppc_i++) {
		for (int cam_i = 0; cam_i < total_num_cameras; cam_i++) {
			if (!PPC[ppc_i]->CheckOcclusion(cam_i)) {
				geo = PPC[ppc_i]->GetGeometry();

				p.x = geo[0];
				p.y = geo[1];
				p.z = geo[2];
				
				color = PPC[ppc_i]->GetColor(cam_i);

				p.r = color[0];
				p.g = color[1];
				p.b = color[2];

				projection_bypoint(p, cam_i, proj_img_vec[cam_i], depth_value_imgs[cam_i], is_hole_proj_imgs[cam_i]);
			}
		}
	}
}

void projection_PPC_with_hole_filling(
	vector<PPC*> Plen_PC,
	vector<Mat> &projection_imgs,
	vector<Mat> &filled_imgs,
	vector<Mat> &is_hole_proj_imgs,
	vector<Mat> &is_hole_filled_imgs,
	vector<PointCloud<PointXYZRGB>::Ptr> &pointclouds_,
	int nNeighbor,
	int window_size)
{
	cout << "projection and hole filling is proceeding... " << endl;
	make_proj_img_vec_ppc2(Plen_PC, projection_imgs, is_hole_proj_imgs, pointclouds_, nNeighbor);
	is_hole_filled_imgs = is_hole_proj_imgs;
	hole_filling_PPC(projection_imgs, filled_imgs, is_hole_filled_imgs, window_size);
	cout << "projection and hole filling done... " << endl;
}

void hole_filling_PPC(
	vector<Mat> proj_imgs,
	vector<Mat> &filled_imgs,
	vector<Mat>& is_hole_filled_imgs,
	int window_size)
{
	Mat hole_image;

	for (int num = 0; num < total_num_cameras; num++) {
		//hole_image = find_hole_PPC(proj_imgs[num]); //hole -> 0
		filled_imgs[num] = make_filled_image_PPC(proj_imgs[num], is_hole_filled_imgs[num], window_size);
	}
}

Mat find_hole_PPC(Mat projection_img)
{
	Mat hole_image(_height, _width, CV_8U, 1);
	for (int rownum = 0; rownum < _height; rownum++)
	{
		for (int colnum = 0; colnum < _width; colnum++)
		{
			if (projection_img.at<Vec3b>(rownum, colnum) == Vec3b(0, 0, 0))
				hole_image.at<uchar>(rownum, colnum) = 0;
		}
	}
	return hole_image;
}

Mat make_filled_image_PPC(Mat colorimg, Mat& hole_image, int window_size){
	//hole_image 
	//	0-projection ok
	//	1-hole
	//	2-not projection but filled

	Mat filled_image(_height, _width, CV_8UC3, Scalar::all(0));

	for (int rownum = 0; rownum < _height; rownum++){
		for (int colnum = 0; colnum < _width; colnum++){
			bool is_not_hole;
			if (hole_image.at<uchar>(rownum, colnum) == 0) is_not_hole = true;
			else is_not_hole = false;
			//bool is_not_hole = !(hole_image.at<bool>(rownum, colnum));

			if (is_not_hole)
			{
				filled_image.at<Vec3b>(rownum, colnum)[0] = colorimg.at<Vec3b>(rownum, colnum)[0];
				filled_image.at<Vec3b>(rownum, colnum)[1] = colorimg.at<Vec3b>(rownum, colnum)[1];
				filled_image.at<Vec3b>(rownum, colnum)[2] = colorimg.at<Vec3b>(rownum, colnum)[2];
			}
			else
			{
				vector<ushort> pixel_sum(3);
				int pixel_count = 0;

				for (int h = rownum - window_size; h <= rownum + window_size; h++)
				{
					for (int w = colnum - window_size; w <= colnum + window_size; w++)
					{
						if (h < 0 || w < 0 || h >= _height || w >= _width) continue;
						else if (hole_image.at<uchar>(h, w)!=0) continue;
						else
						{
							pixel_sum[0] += colorimg.at<Vec3b>(h, w)[0];
							pixel_sum[1] += colorimg.at<Vec3b>(h, w)[1];
							pixel_sum[2] += colorimg.at<Vec3b>(h, w)[2];
							pixel_count++;
						}
					}
				}

				if (pixel_count == 0)
				{
					filled_image.at<Vec3b>(rownum, colnum)[0] = 0;
					filled_image.at<Vec3b>(rownum, colnum)[1] = 0;
					filled_image.at<Vec3b>(rownum, colnum)[2] = 0;
				}
				else
				{
					filled_image.at<Vec3b>(rownum, colnum)[0] = uchar(pixel_sum[0] / pixel_count);
					filled_image.at<Vec3b>(rownum, colnum)[1] = uchar(pixel_sum[1] / pixel_count);
					filled_image.at<Vec3b>(rownum, colnum)[2] = uchar(pixel_sum[2] / pixel_count);
					hole_image.at<uchar>(rownum, colnum) = 2;
				}
			}
		}
	}
	return filled_image;
}

void save_ppc(vector<PPC*> ppc, string filename) {

	ofstream fout(filename, ios::binary);

	for (vector<PPC*>::iterator vit = ppc.begin(), vend = ppc.end(); vit != vend; vit++){
		float* geo = new float(3);
		geo = (*vit)->GetGeometry();
		fout.write((char*)&geo[0], sizeof(float));
		fout.write((char*)&geo[1], sizeof(float));
		fout.write((char*)&geo[2], sizeof(float));

		if (version == 2.1) {
			uchar refV = (*vit)->GetrefV();
			uchar refU = (*vit)->GetrefU();
			fout.write((char*)&refV, sizeof(uchar));
			fout.write((char*)&refU, sizeof(uchar));

			vector<uchar> VU = (*vit)->GetVU();
			vector<uchar> Y = (*vit)->GetY();

			for (int i = 0; i < total_num_cameras; i++) {
				fout.write((char*)&VU[i], sizeof(uchar));
			}
			for (int i = 0; i < total_num_cameras; i++) {
				fout.write((char*)&Y[i], sizeof(uchar));
			}
		}
		else if (version == 2.2) {
			uchar avrV = (*vit)->GetV();
			uchar avrU = (*vit)->GetU();
			fout.write((char*)&avrV, sizeof(uchar));
			fout.write((char*)&avrU, sizeof(uchar));

			vector<uchar> Y = (*vit)->GetY();
			vector<bool> occ = (*vit)->GetOcclusion();
		
			fout.write((char*)&Y[0], total_num_cameras * sizeof(uchar));
			for (int i = 0; i < total_num_cameras; i++) {
				char* temp;
				if (i % 8 == 0) {
					temp = new char;
					*temp = occ[i];
				}
				else if (i % 8 == 7 ) {
					*temp <<= 1;
					*temp |= occ[i];
					fout.write((char*)temp, sizeof(char));
					delete temp;
				}
				else if (i == total_num_cameras - 1) {
					*temp <<= 1;
					*temp |= occ[i];
					*temp <<= 8 - (total_num_cameras % 8);
					fout.write((char*)temp, sizeof(char));
					delete temp;
				}
				else {
					*temp <<= 1;
					*temp |= occ[i];
				}
			}
		}
	}

	fout.close();
	cout << "save pcc done..." << endl << endl;
}

vector<PPC*> load_ppc(string filename) {
	vector<PPC*> vec_ppc;
	ifstream fin(filename, ios::binary);

	while (!fin.eof()) {
		PPC* pc;
		if (version == 2.1) pc = new PPC_v2_1;
		else if (version == 2.2) pc = new PPC_v2_2;

		float* geo = (float*)malloc(3 * sizeof(float));
		fin.read((char*)(&geo[0]), sizeof(float));
		fin.read((char*)(&geo[1]), sizeof(float));
		fin.read((char*)(&geo[2]), sizeof(float));
		pc->SetGeometry(geo);

		if (version == 2.1) {
			uchar refV, refU;
			fin.read((char*)(&refV), sizeof(uchar));
			fin.read((char*)(&refU), sizeof(uchar));

			vector<uchar> VU(total_num_cameras), Y(total_num_cameras);
			for (int i = 0; i < total_num_cameras; i++) {
				fin.read((char*)&VU[i], sizeof(uchar));
			}
			for (int i = 0; i < total_num_cameras; i++) {
				fin.read((char*)&Y[i], sizeof(uchar));
			}
			pc->SetColor(refV, refU, VU, Y);
		}
		else if (version == 2.2) {
			uchar avrV, avrU;
			fin.read((char*)(&avrV), sizeof(uchar));
			fin.read((char*)(&avrU), sizeof(uchar));

			vector<uchar> Y(total_num_cameras);
			vector<bool> occlusion(total_num_cameras);

			for (int i = 0; i < total_num_cameras; i++) {
				fin.read((char*)&Y[i], sizeof(uchar));
			}

			for (int i = 0; i < total_num_cameras; i++) {
				char* temp;
				if (i % 8 == 0) {
					temp = new char;
					fin.read((char*)temp, sizeof(char));
					unsigned char t = *temp & 128; //1000 0000
					if (t == 128) {
						occlusion[i] = true;
					}
					else occlusion[i] = false;
					*temp <<= 1;
				}
				else if (i % 8 == 7 || i == total_num_frames - 1) {
					unsigned char t = *temp & 128; //1000 0000
					if (t == 128) {
						occlusion[i] = true;
					}
					else occlusion[i] = false;
					delete temp;
				}
				else {
					unsigned char t = *temp & 128; //1000 0000
					if (t == 128) {
						occlusion[i] = true;
					}
					else occlusion[i] = false;
					*temp <<= 1;
				}
			}
			pc->SetColor(avrV, avrU, Y, occlusion);
		}
		vec_ppc.push_back(pc);
	}
	
	fin.close();
	cout << "load pcc done..." << endl << endl;
	return vec_ppc;
}
